#include <stdlib.h>    // for getenv
#include <time.h>

#include <captcha/captcha.h>
#include <stfilter/stfbs64.hpp>
#include <md5/md5.h>

#include "xrandom.h"
#include "xcaptcha.hpp"


/*
   It might look questionable to use md5 in a program written in 2022.
   However, please note that, despite all the rumors that md5 is
   broken completely, noone has yet found the inverse function to it.
   It takes significant amount of time to perform every particular
   "break", although it is, yes, possible.  I once saw two different
   black&white photos of jazz stars having the same md5 hashes.

   However, a single captcha solution lifetime is very limited, so when
   someone manages to break it, definitely it will be too late.

   From the other hand, we work inside a CGI program, which is triggered
   to run by an HTTP request sent from somewhere in Internet, so the
   more computations it performs on every single run, the easier it
   will be to perform Denial of Service on it.  Md5 is notably faster
   than sha256.

   Anyway, this module is the only one that knows about md5 here.  No
   assumptions are made outside of it about a particular hash nor its
   properties (such as hash size).  So if you want to replace md5 with
   sha256 (or whatever other hash), you ONLY need to patch the
   generate_captcha_token function; hopefully all the code outside of
   it will continue to work.  Only one thing to mention: PLEASE PLEASE
   PLEASE PLEASE don't make this program depend on anything external
   such as cryptographic libraries.
 */




static ScriptVariable the_secret(0);
static int the_ttl = -1;
static char *the_image;
static int the_image_size;
static char the_answer[CAPTCHA_STRING_LENGTH+1];
static ScriptVariable the_captcha_time(0);
static ScriptVariable the_captcha_nonce(0);

static void generate_if_not_yet()
{
    if(the_image)
        return;
    generate_captcha(&the_image, &the_image_size, the_answer);
}


void set_captcha_info(const ScriptVariable &secret, int time_to_live)
{
    the_secret = secret;
    the_ttl = time_to_live;
}

ScriptVariable captcha_ip()
{
    return getenv("REMOTE_ADDR");
}

ScriptVariable captcha_time()
{
    if(the_captcha_time.IsInvalid() || the_captcha_time == "")
        the_captcha_time = ScriptNumber(time(0));
    return the_captcha_time;
}

static ScriptVariable take_rev_hex(unsigned long long n)
{
    ScriptVariable res;
    while(n) {
        int d = n & 0x0f;
        n >>= 4;
        char c = d < 10 ? '0' + d : 'A' - 10 + d;
        res += c;
    }
    return res;
}

ScriptVariable captcha_nonce()
{
    if(the_captcha_nonce.IsInvalid() || the_captcha_nonce == "") {
        unsigned long long num;
        fill_random(&num, sizeof(num));
        the_captcha_nonce = take_rev_hex(num);
    }
    return the_captcha_nonce;
}

class FDest : public StreamFilter {
public:
    ScriptVariable res;
    FDest() : StreamFilter(0) {}
    ~FDest() {}
    virtual void FeedChar(int c) { res.operator+=((char)c); } // no EOF, ok
};

static ScriptVariable take_base64(const void *buf, int len)
{
    FDest dest;
    StreamFilterBase64encode enc(&dest);
    int i;
    for(i = 0; i < len; i++)
        enc.FeedChar(((unsigned char*)buf)[i]);   // unsigned, ok
    enc.FeedEnd();
    return dest.res;
}


ScriptVariable captcha_image_base64()
{
    generate_if_not_yet();
    return take_base64(the_image, the_image_size);
}

static ScriptVariable generate_captcha_token(const ScriptVariable &ip,
                                             const ScriptVariable &tm,
                                             const ScriptVariable &nonce,
                                             const ScriptVariable &resp)
{
    ScriptVariable src = the_secret + ip + tm + nonce + resp;
    if(src.IsInvalid())
        return ScriptVariableInv();
    unsigned char digest[MD5_DIGEST_SIZE];
    MD5StringHash(digest, src.c_str());
    return take_base64(digest, sizeof(digest));
}

ScriptVariable captcha_token()
{
    generate_if_not_yet();
    ScriptVariable ans(the_answer);
    ans.Tolower();
    return generate_captcha_token(captcha_ip(), captcha_time(),
                                  captcha_nonce(), ans);
}

int captcha_validate(const ScriptVariable &form_ip,
                     const ScriptVariable &form_time,
                     const ScriptVariable &form_nonce,
                     const ScriptVariable &form_token,
                     const ScriptVariable &form_response)
{
    if(form_ip != captcha_ip())
        return captcha_result_ip_mismatch;

    long long ft;
    bool tm_ok = form_time.GetLongLong(ft, 10);
    if(!tm_ok)
        return captcha_result_broken_data;

    long long tt = time(0);
    if(ft > tt)
        return captcha_result_broken_data;
    if(tt - ft > the_ttl)
        return captcha_result_expired;

    ScriptVariable ans = form_response;
    ans.Tolower();
    ScriptVariable tok =
        generate_captcha_token(form_ip, form_time, form_nonce, ans);
    if(form_token != tok)
        return captcha_result_wrong;

    return captcha_result_ok;
}

