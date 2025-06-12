#include "urlenc.hpp"

static int hexdig(int v)
{
    v &= 0x0f;
    return v <= 9 ? v + '0' : v + 'A' - 10;
}

static const char *percent_enc_char(int c)
{
    static char res[4] = "%..";
    res[1] = hexdig(c >> 4);
    res[2] = hexdig(c);
    return res;
}

static int hexdigvalue(int c)
{
    if(c >= '0' && c <= '9')
        return c - '0';
    if(c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if(c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

ScriptVariable url_encode(const ScriptVariable &plain_sv)
{
    ScriptVariable res;
    const char *plain = plain_sv.c_str();
    for(; *plain; plain++) {
        if(*plain == ' ') {
            res += '+';
        } else
        if((*plain >= 'A' && *plain <= 'Z') ||
            (*plain >= 'a' && *plain <= 'z') ||
            (*plain >= '0' && *plain <= '9') ||
            *plain == '-' || *plain == '_' || *plain == '~' || *plain == '.')
        {
            res += *plain;
        } else {
            res += percent_enc_char(*plain);
        }
    }
    return res;
}

static const char *decoded_byte(const char *enc)
{
    static char buf[4] = "\0\0\0";
    int dig1 = hexdigvalue(*enc);
    int dig2 = hexdigvalue(enc[1]);
    if(dig1 == -1 || dig2 == -1) {
        buf[0] = '%';
        buf[1] = *enc;
        buf[2] = enc[1];   // NB: buf[3] never changes
    } else {
        buf[0] = (dig2 & 0x0f) | ((dig1 << 4) & 0xf0);
        buf[1] = 0;
    }
    return buf;
}

ScriptVariable url_decode(const ScriptVariable &encoded_sv)
{
    ScriptVariable res;
    const char *encoded = encoded_sv.c_str();
    while(*encoded) {
        if(*encoded == '+') {
            res += ' ';
            encoded++;
        } else
        if(*encoded == '%') {
            if(encoded[1] && encoded[2]) {
                res += decoded_byte(encoded+1);
                encoded += 3;
            } else {
                // unexpected end of line; leave %_ as is
                res += encoded;
                break;
            }
        } else {
            res += *encoded;
            encoded++;
        }
    }
    return res;
}


