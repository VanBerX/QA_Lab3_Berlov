#ifndef XCGI_HPP_SENTRY
#define XCGI_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>


class FileUploadReceiver {
public:
    virtual ~FileUploadReceiver() {}
    virtual bool FileStart(const ScriptVariable &input_name,
                           const ScriptVariable &filename,
                           const ScriptVariable &mime_type) = 0;
    virtual bool FilePortion(const char *buf, int len) = 0;
    virtual bool FileEnd() = 0;
        // returning false from any of these means stop reading immediately
};



class Cgi {
        /* response */
    int status_code;
    ScriptVariable status_message;
    ScriptVariable response_body;
    ScriptVariable location;
    ScriptVector setcookie_strings;

        /* request */
    ScriptVector params;
    ScriptVector cookies;
    long long content_length;
public:
    Cgi();
    ~Cgi();

    bool ParseHead();
    long long ContentLength() const { return content_length; }
    bool BodyExpected() const { return content_length > 0; }
    bool ParseBody();

    void GetStatus(int &code, ScriptVariable &msg) const;
    bool IsPost() const;
    ScriptVariable GetPath() const;
    ScriptVariable GetScript() const;
    ScriptVariable GetDocRoot() const;
    ScriptVariable GetAddr() const;
    ScriptVariable GetHost() const;
    int GetPort() const;
    ScriptVariable GetCookie(const ScriptVariable &name) const;
    ScriptVariable GetParam(const ScriptVariable &name) const;

    ScriptVariable GetRemoteHost() const;
    ScriptVariable GetRemoteAddr() const;
    int GetRemotePort() const;

    void SetStatus(int code, const ScriptVariable &msg);
        // set 303 "See Other" and the given location
    void SetSeeOther(const ScriptVariable &location);

    void SetBody(const ScriptVariable &s);
    void SetCookie(const ScriptVariable &name,
                   const ScriptVariable &value,
                   int ttl, bool httponly, bool secure);
    void DiscardCookie(const ScriptVariable &name);

    void Commit();

private:
    bool DoParseHead();
    bool DoParseBody();
};

#endif
