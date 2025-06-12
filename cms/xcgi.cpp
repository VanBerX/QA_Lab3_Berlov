#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "urlenc.hpp"

#include "xcgi.hpp"



Cgi::Cgi()
    : status_code(200), status_message("Ok"), response_body(""), location(0),
    content_length(-1)
{
}

Cgi::~Cgi()
{
}

static void query_to_params(const char *query, ScriptVector &params)
{
    fprintf(stderr, "QUERY: [%s]\n", query);
    ScriptVector v(query, "&", "");
    int vl = v.Length();
    int i;
    for(i = 0; i < vl; i++) {
        if(v[i].IsInvalid() || v[i] == "")
            continue;
        ScriptVariable::Substring eqpos = v[i].Strchr('=');
        ScriptVariable name, value;
        if(eqpos.IsValid()) {
            name = url_decode(eqpos.Before().Get());
            value = url_decode(eqpos.After().Get());
        } else {
            name = url_decode(v[i]);
            value = "";
        }
        params.AddItem(name);
        params.AddItem(value);
        fprintf(stderr, "PARAM: [%s]=[%s]\n", name.c_str(), value.c_str());
    }
}

static void extract_cookies(const char *str, ScriptVector &cookies)
{
    fprintf(stderr, "COOKIE-STRING: [%s]\n", str);
    ScriptVector v(str, ";", " \t");
    int vl = v.Length();
    int i;
    for(i = 0; i < vl; i++) {
        if(v[i].IsInvalid() || v[i].Trim() == "")
            continue;
        ScriptVariable::Substring eqpos = v[i].Strchr('=');
        ScriptVariable name, value;
        if(eqpos.IsValid()) {
            name = eqpos.Before().Get().Trim();
            value = eqpos.After().Get().Trim();
        } else {
            name = "";
            value = v[i];
        }
        cookies.AddItem(name);
        cookies.AddItem(value);
        fprintf(stderr, "COOKIE: [%s]=[%s]\n", name.c_str(), value.c_str());
    }

}

bool Cgi::DoParseHead()
{
    const char *tmp;
    tmp = getenv("QUERY_STRING");
    if(!tmp)                      // CGI protocol's broken!
        return false;
    query_to_params(tmp, params);

    tmp = getenv("HTTP_COOKIE");
    if(tmp)
        extract_cookies(tmp, cookies);

    tmp = getenv("CONTENT_LENGTH");
    if(tmp) {
        bool ok = ScriptVariable(tmp).GetLongLong(content_length, 10);
        if(!ok)
            content_length = -1;
    } else {
        content_length = -1;
    }

    return true;
}

bool Cgi::ParseHead()
{
    bool ok = DoParseHead();
    if(!ok)
        SetStatus(400, "broken request (head)");
    return ok;
}

static const char multipart[] = "multipart/form-data;";

bool Cgi::DoParseBody()
{
    const char *tmp = getenv("CONTENT_TYPE");
    if(tmp) {
        ScriptVariable preftp =
            ScriptVariable(tmp).Range(0, sizeof(multipart)-1).Get().Tolower();
        if(preftp == multipart) {
            // XXXXX multipart not implemented yet
            return false;
        }
    }
    // in all other cases assume it is x-url-encoded
    if(!BodyExpected())
        return false;

    char *buf = new char[content_length+1];

    int rt = 0;
    do {
        int to_rd = content_length - rt;
        if(to_rd > 4096)
            to_rd = 4096;
        int r = read(0, buf+rt, to_rd);
        if(r < 1) {
            delete[] buf;
            return false;
        }
        rt += r;
    } while(rt < content_length);

    buf[content_length] = 0;
    query_to_params(buf, params);
    delete[] buf;
    return true;
}

bool Cgi::ParseBody()
{
    bool ok = DoParseBody();
    if(!ok)
        SetStatus(400, "broken request (body)");
    return ok;
}


void Cgi::GetStatus(int &code, ScriptVariable &msg) const
{
    code = status_code;
    msg = status_message;
}

bool Cgi::IsPost() const
{
    const char *c = getenv("REQUEST_METHOD");
    if(!c)
        return false;
    return ScriptVariable(c).Toupper() == "POST";
}

ScriptVariable Cgi::GetPath() const
{
    const char *pt = getenv("PATH_INFO");
    if(!pt || !*pt)
        pt = "/";
    return pt;
}

static const char *getenv_e(const char *s)
{
    const char *r = getenv(s);
    return r ? r : "";
}

ScriptVariable Cgi::GetDocRoot() const
{
    return getenv_e("DOCUMENT_ROOT");
}

ScriptVariable Cgi::GetScript() const
{
    return getenv_e("SCRIPT_NAME");
}

ScriptVariable Cgi::GetHost() const
{
    return getenv_e("HTTP_HOST");
}

int Cgi::GetPort() const
{
    ScriptVariable p(getenv_e("SERVER_PORT"));
    long n;
    if(!p.GetLong(n, 10) || n <= 0 || n >= 65535)
        return -1;
    return n;
}

ScriptVariable Cgi::GetRemoteHost() const
{
    return getenv_e("REMOTE_HOST");
}

ScriptVariable Cgi::GetRemoteAddr() const
{
    return getenv_e("REMOTE_ADDR");
}

int Cgi::GetRemotePort() const
{
    ScriptVariable p(getenv_e("REMOTE_PORT"));
    long n;
    if(!p.GetLong(n, 10) || n <= 0 || n >= 65535)
        return -1;
    return n;
}


static ScriptVariable dict_search(const ScriptVariable &name,
                                  const ScriptVector &dict)
{
    int dl = dict.Length();
    int i;
    for(i = 0; i < dl-1; i+=2)
        if(dict[i] == name)
            return dict[i+1];
    return ScriptVariableInv();
}

ScriptVariable Cgi::GetCookie(const ScriptVariable &name) const
{
    return dict_search(name, cookies);
}

ScriptVariable Cgi::GetParam(const ScriptVariable &name) const
{
    return dict_search(name, params);
}

void Cgi::SetStatus(int code, const ScriptVariable &msg)
{
    status_code = code;
    status_message = msg;
}

    // set 303 "See Other" and the given location
void Cgi::SetSeeOther(const ScriptVariable &loc)
{
    SetStatus(303, "See Other");
    location = loc;
}

void Cgi::SetBody(const ScriptVariable &s)
{
    response_body = s;
}

void Cgi::SetCookie(const ScriptVariable &name,
                    const ScriptVariable &value,
                    int ttl, bool httponly, bool secure)
{
    ScriptVariable res = name + "=" + value + "; Path=/";
    if(ttl > 0) {   // okay, use DiscardCookies to remove the cookie!
        res += "; Max-Age=";
        res += ScriptNumber(ttl);
    }
    if(httponly)
        res += "; HttpOnly";
    if(secure)
        res += "; Secure";
    setcookie_strings.AddItem(res);
}

void Cgi::DiscardCookie(const ScriptVariable &name)
{
    ScriptVariable res =
        name + "= ; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT; Max-Age=0";
    setcookie_strings.AddItem(res);
}

void Cgi::Commit()
{
    printf("Status: %d %s\r\n"
           "Content-Type: text/html\r\n",
           status_code, status_message.c_str());
    int i;
    for(i = 0; i < setcookie_strings.Length(); i++) {
        fputs("Set-Cookie: ", stdout);
        fputs(setcookie_strings[i].c_str(), stdout);
        fputs("\r\n", stdout);
    }
    if(location.IsValid())
        printf("Location: %s\r\n", location.c_str());
    bool have_body = response_body.IsValid() && response_body.Length() > 0;
    // XXXXX may be send the body length header?
    fputs("\r\n", stdout);   // finish the header
    if(have_body)
        fputs(response_body.c_str(), stdout);
}

