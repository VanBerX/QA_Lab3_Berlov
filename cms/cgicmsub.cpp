#include <stdlib.h>    // for getenv

#include "xcgi.hpp"

#include "cgicmsub.hpp"



class VarRequestData : public ScriptMacroprocessorMacro {
    const Cgi *req;
public:
    VarRequestData(const Cgi *c)
        : ScriptMacroprocessorMacro("req", true), req(c) {}
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
};

ScriptVariable VarRequestData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarRequestData::DoExpand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "method")
        return req->IsPost() ? "POST" : "GET";
    if(s == "document_root")
        return req->GetDocRoot();
    if(s == "host")
        return req->GetHost();
    if(s == "port")
        return ScriptNumber(req->GetPort());
    if(s == "script")
        return req->GetScript();
    if(s == "path")
        return req->GetPath();
    if(s == "param") {
        if(params[1].IsInvalid() || params[1] == "")
            return "";
        return req->GetParam(params[1]);
    }
    if(s == "cookie") {
        if(params[1].IsInvalid() || params[1] == "")
            return "";
        return req->GetCookie(params[1]);
    }
    return ScriptVariable("[req:") + s + "?!]";
}

///////////////////////////////////////////////////////////////////////////

class VarGetEnv : public ScriptMacroprocessorMacro {
public:
    VarGetEnv() : ScriptMacroprocessorMacro("getenv", true /*dirty*/) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarGetEnv::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1 || params[0].IsInvalid())
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    const char *tmp = getenv(s.c_str());
    return tmp ? tmp : "";
}

///////////////////////////////////////////////////////////////////////////


CommonCgiSubstitutions::CommonCgiSubstitutions(const Cgi *cgi)
    : BaseSubstitutions("")
{
    AddMacro(new VarGetEnv);                              // [getenv: ]
    AddMacro(new VarRequestData(cgi));                    // [req: ]
}

CommonCgiSubstitutions::~CommonCgiSubstitutions()
{
}

