#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrmacro.hpp>
#include <scriptpp/cmd.hpp>

#include <inifile/inifile.hpp>

#include "cgicmsub.hpp"
#include "xcgi.hpp"
#include "xrandom.h"


#include "dullcgi.hpp"



////////////////////////////////////////////////////////////////
// The three objects expected to be implemented and exported
// by the user-supplied module
//

extern const char *dullcgi_config_path;
extern const char *dullcgi_program_name;
int dullcgi_main(DullCgi *cgi);


/////////////////////////////////////////////////////////////////
// The database and the macroprocessor class definitions
//

class DullCgiDb;

class DullCgiDbSubstitution : public CommonCgiSubstitutions {
    const DullCgiDb *the_database;
public:
    DullCgiDbSubstitution(const DullCgiDb *master, const Cgi *cgi);
    ~DullCgiDbSubstitution();
};

class VarHtmlSection : public ScriptMacroprocessorMacro {
    const DullCgiDbSubstitution *the_master;
    const DullCgiDb *the_database;
public:
    VarHtmlSection(const DullCgiDbSubstitution *m, const DullCgiDb *db)
        : ScriptMacroprocessorMacro("html"), the_master(m), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

class DullCgiDb {
    class IniFileParser *inifile;
    class DullCgiDbSubstitution *subst;
    ScriptVector aux_args;
public:
    DullCgiDb(const Cgi *cgi);
    ~DullCgiDb();

    void AddMacro(class ScriptMacroprocessorMacro *p);
    void SetPositionals(const ScriptVector &v) { aux_args = v; }
    ScriptVariable GetConfigValue(const ScriptVariable &sect1,
                                  const ScriptVariable &sect2,
                                  const ScriptVariable &param,
                                  const ScriptVariable &qualif,
                                  bool expand_macros) const;
    int GetSectionNames(const ScriptVariable &group_id,
                        class ScriptVector &names) const;

    bool Load(const ScriptVariable &filename);
    ScriptVariable MakeErrorMessage() const;
    ScriptVariable GetHtmlSnippet(const ScriptVariable &name) const;
    ScriptVariable MakeErrorPage(int code, const ScriptVariable &cmt) const;
    ScriptVariable BuildPage(const char *pageid) const;
    ScriptVariable BuildRedirectPage(const ScriptVariable &url) const;

    ScriptMacroprocessor *GetMacroprocessor() const { return subst; }
};


/////////////////////////////////////////////////////////////////
// The database and the macroprocessor implementation
//

DullCgiDbSubstitution::DullCgiDbSubstitution(const DullCgiDb *m, const Cgi *c)
    : CommonCgiSubstitutions(c), the_database(m)
{
    AddMacro(new VarHtmlSection(this, m));                // [html: ]
}

DullCgiDbSubstitution::~DullCgiDbSubstitution()
{
}

/////////////////////////////////////


DullCgiDb::DullCgiDb(const Cgi *cgi)
{
    inifile = new IniFileParser;
    subst = new DullCgiDbSubstitution(this, cgi);
}

DullCgiDb::~DullCgiDb()
{
    delete subst;
    delete inifile;
}

void DullCgiDb::AddMacro(class ScriptMacroprocessorMacro *p)
{
    subst->AddMacro(p);
}

bool DullCgiDb::Load(const ScriptVariable &filename)
{
    return inifile->Load(filename.c_str());
}

ScriptVariable DullCgiDb::MakeErrorMessage() const
{
    ScriptVariable msg("");
    int el = inifile->GetLastErrorLine();
    if(el != -1)
        msg += ScriptNumber(el) + ": ";
    else
        msg += " ";
    msg += inifile->GetLastErrorDescription();
    return msg;
}

ScriptVariable DullCgiDb::GetHtmlSnippet(const ScriptVariable &name) const
{
    return inifile->GetTextParameter("html", 0, name.c_str(), "");
}

static const char *c_str_if_notempty(const ScriptVariable &v)
{
    if(v.IsInvalid() || v == "")
        return 0;
    return v.c_str();
}

ScriptVariable DullCgiDb::GetConfigValue(const ScriptVariable &sect1,
                                         const ScriptVariable &sect2,
                                         const ScriptVariable &param,
                                         const ScriptVariable &qualif,
                                         bool expand_macros) const
{
    const char *qs = c_str_if_notempty(qualif);
    const char *tmp = qs ?
        inifile->GetModifiedTextParameter(sect1.c_str(),
                                          c_str_if_notempty(sect2),
                                          param.c_str(),
                                          qs,
                                          0)
        :
        inifile->GetTextParameter(sect1.c_str(),
                                  c_str_if_notempty(sect2),
                                  param.c_str(),
                                  0);
    if(!tmp)
        return ScriptVariableInv();
    if(!expand_macros)
        return tmp;
    return subst->Process(tmp, aux_args);
}

int DullCgiDb::GetSectionNames(const ScriptVariable &group_id,
                               class ScriptVector &names) const
{
    const char *gn = group_id.c_str();
    int cnt = inifile->GetSectionCount(gn);
    names.Clear();
    int i;
    for(i = 0; i < cnt; i++)
        names[i] = inifile->GetSectionName(gn, i);
    return cnt;
}

static const char default_error_page_template[] =
    "<?xml version=\"1.0\" encoding=\"US-ASCII\" ?>\n"
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n"
    "  \"http://www.w3.org/TR/xhtml1/DTD/strict.dtd\">\n"
    "<html xmlns=\"http://www.w3.org/TR/xhtml1/strict\" >\n"
    "<head><title>%progname% default error page</title></head>\n"
    "<body><h1>%progname% default error page</h1>\n"
    "<h2>%errcode% %errmessage%</h2>\n"
    "<p>If you see this page it probably means the configuration\n"
    "file for %progname% CGI program is either missing or\n"
    "broken.  Please check.</p></body></html>\n";


ScriptVariable
DullCgiDb::MakeErrorPage(int code, const ScriptVariable &comment) const
{
    ScriptVariable templ =
        inifile->GetTextParameter("errorpage", 0, "template",
                                  default_error_page_template);
    ScriptNumber code_s(code);
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("progname", dullcgi_program_name));
    sub_sub.AddMacro(new ScriptMacroConst("errcode", code_s));
    sub_sub.AddMacro(new ScriptMacroConst("errmessage", comment));
    return sub_sub.Process(templ);
}

ScriptVariable DullCgiDb::BuildPage(const char *pgid) const
{
    const char *tmp;

#if 0
    tmp = inifile->GetTextParameter("page", pgid, "selector", 0);
    if(tmp) {
        data.selector = subst->Process(tmp, aux_args);
        data.selector.Trim();
    } else {
        data.selector = ScriptVariableInv();
    }
#endif

    tmp = inifile->GetTextParameter("page", pgid, "template", 0);
    if(!tmp)
        return ScriptVariable("No template found for ") + pgid + "\n";
    ScriptVariable res = subst->Process(tmp, aux_args);
    return res;
}

ScriptVariable DullCgiDb::BuildRedirectPage(const ScriptVariable &url) const
{
    const char *tmp;
    tmp = inifile->GetTextParameter("redirectpage", 0, "template", 0);
    if(!tmp)
        return "No template configured for the redirect page";
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("url", url));
    return sub_sub.Process(tmp);
}

/////////////////////////////////////////////////////////////////
//

ScriptVariable VarHtmlSection::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable sn = the_database->GetHtmlSnippet(params[0].Trim());
    return the_master->Process(sn, params, 1, len-1);
}

//
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
// DullCgi methods implementation
//

void DullCgi::AddMacro(class ScriptMacroprocessorMacro *p)
{
    if(!the_database)
        return;
    the_database->AddMacro(p);
}

void DullCgi::SetPositionals(const ScriptVector &v)
{
    if(!the_database)
        return;
    the_database->SetPositionals(v);
}

ScriptMacroprocessor *DullCgi::MakeMacroprocessorClone() const
{
    if(!the_database)
        return new ScriptMacroprocessor;
    return new ScriptMacroprocessor(the_database->GetMacroprocessor());
}

void DullCgi::DisposeMacroprocessorClone(ScriptMacroprocessor *p)
{
    delete p;
}

ScriptVariable DullCgi::GetConfigValue(const ScriptVariable &sect1,
                                       const ScriptVariable &sect2,
                                       const ScriptVariable &par,
                                       const ScriptVariable &qual,
                                       bool expand) const
{
    if(!the_database)
        return ScriptVariableInv();
    return the_database->GetConfigValue(sect1, sect2, par, qual, expand);
}

int DullCgi::GetSectionNames(const ScriptVariable &group_id,
                             class ScriptVector &names) const
{
    if(!the_database)
        return -1;
    return the_database->GetSectionNames(group_id, names);
}

void DullCgi::SendErrorPage(int code, const ScriptVariable &cmt)
{
    ScriptVariable pg = the_database->MakeErrorPage(code, cmt);
    SetStatus(code, cmt);
    SetBody(pg);
    Commit();
}

void DullCgi::SendPage(const ScriptVariable &pgid)
{
    ScriptVariable pg = the_database->BuildPage(pgid.c_str());
    SetBody(pg);
    Commit();
}

void DullCgi::SendRedirect(const ScriptVariable &url)
{
    ScriptVariable pg = the_database->BuildRedirectPage(url);
    SetBody(pg);
    SetSeeOther(url);
    Commit();
}

//
/////////////////////////////////////////////////////////////////






/////////////////////////////////////////////////////////////////
// the main function

int main()
{
    DullCgi cgi;
    DullCgiDb db(&cgi);
    cgi.SetDatabase(&db);

    if(!db.Load(dullcgi_config_path)) {
        ScriptVariable diag = db.MakeErrorMessage();
        cgi.SendErrorPage(500, diag.c_str());
        return 0;
    }

    randomize();

    if(!cgi.ParseHead()) {
        cgi.Commit();
        return 0;
    }
    if(cgi.IsPost()) {
        cgi.SendErrorPage(405, "Method not allowed");
        return 0;
    }

    return dullcgi_main(&cgi);
}
