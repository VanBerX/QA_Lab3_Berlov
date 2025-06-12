#include <stdlib.h>   // for rand
#include <inifile/inifile.hpp>

#include "xcaptcha.hpp"
#include "tcgi_sub.hpp"
#include "tcgi_rpl.hpp"
#include "tcgi_ses.hpp"
#include "makeargv.hpp"
#include "roles.hpp"
#include "fnchecks.h"

#include "tcgi_db.hpp"

#ifndef THALASSA_DEFAULT_USERDATA_DIR
#define THALASSA_DEFAULT_USERDATA_DIR "/var/local/thalassa"
#endif


ThalassaCgiDb::ThalassaCgiDb(const Cgi *cgi)
    : the_session(0), access_checker(0)
{
    inifile = new IniFileParser;
    subst = new ThalassaCgiDbSubstitution(this, cgi);
}

ThalassaCgiDb::~ThalassaCgiDb()
{
    delete subst;
    delete inifile;
    if(access_checker)
        delete access_checker;
}

bool ThalassaCgiDb::Load(const ScriptVariable &filename)
{
    conf_file = filename;
    return inifile->Load(filename.c_str());
}

ScriptVariable ThalassaCgiDb::MakeErrorMessage() const
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

void ThalassaCgiDb::
GetFormatData(const char *&enc, const char *&tags, const char *&attrs) const
{
    static const char defattrs[] = "a=href img=src img=alt";
    enc   = inifile->GetTextParameter("format", 0, "encoding", 0);
    tags  = inifile->GetTextParameter("format", 0, "tags", "");
    attrs = inifile->GetTextParameter("format", 0, "tag_attributes", defattrs);
}

ScriptVariable ThalassaCgiDb::GetUserdataDirectory() const
{
    return inifile->GetTextParameter("general", 0, "userdata_dir",
                                     THALASSA_DEFAULT_USERDATA_DIR);
}

#if 0
ScriptVariable ThalassaCgiDb::GetSessionsDirectory() const
{
    ScriptVariable d = GetUserdataDirectory();
    if(d != "" && d[d.Length()-1] != '/')
        d += '/';
    return d + "_sessions";
}
#endif

void ThalassaCgiDb::
GetCaptchaParameters(ScriptVariable &secret, int &time_to_live) const
{
    const char *tmp =
        inifile->GetTextParameter("captcha", 0, "secret", 0);
    if(tmp)
        secret = tmp;
    else
        secret = ScriptNumber(rand());  // so it won't work!
                    // please note we break it intentionally
                    // well, the secret MUST be set in the config
                    // otherwise, the captcha is easily circumvented
    time_to_live =
        inifile->GetIntegerParameter("captcha", 0, "expire", 300);
}

ScriptVariable ThalassaCgiDb::GetHtmlSnippet(const ScriptVariable &name) const
{
    return inifile->GetTextParameter("html", 0, name.c_str(), "");
}

ScriptVariable ThalassaCgiDb::GetContactCategories() const
{
    return inifile->GetTextParameter("feedback", 0, "categories", "");
}

ScriptVariable
ThalassaCgiDb::GetContactCatTitle(const ScriptVariable &cg) const
{
    const char *tmp = inifile->
        GetModifiedTextParameter("feedback", 0, "cattitle", cg.c_str(), 0);
    if(tmp)
        return tmp;
    return ScriptVariable("[==") + cg + "==]";
}

ScriptVariable
ThalassaCgiDb::GetContactRecipient(const ScriptVariable &cg) const
{
    const char *c = cg.c_str();
    return inifile->GetModifiedTextParameter("feedback", 0, "email", c, "");
}

static bool boolean_value_from_string(const char *str)
{
    return str && ScriptVariable(str).Trim().Tolower() == "yes";
}

bool ThalassaCgiDb::GetContactCatIsSel(const ScriptVariable &cg) const
{
    const char *tmp = inifile->
        GetModifiedTextParameter("feedback", 0, "selected", cg.c_str(), 0);
    return boolean_value_from_string(tmp);
}

ScriptVariable ThalassaCgiDb::GetContactEnvelopeFrom() const
{
    return inifile->GetTextParameter("feedback", 0, "envelope_from", "");
}

void ThalassaCgiDb::BuildContactFormMessage(const ScriptVariable &receiver,
                          ScriptVector &command, ScriptVariable &input) const
{
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("receiver", receiver));
    ScriptVariable tmp;
    tmp = inifile->GetTextParameter("feedback", 0, "send_command", "");
    bool ok = make_argv(sub_sub(tmp), command);
    if(!ok || command.Length() < 1) {
        command.Clear();
        input.Invalidate();
        return;
    }
    tmp = inifile->GetTextParameter("feedback", 0, "send_data", "");
    tmp.Trim();
    if(tmp == "") {
        input.Invalidate();
        return;
    }
    input = sub_sub(tmp);
}

void ThalassaCgiDb::BuildServiceEmail(const ScriptVariable &serv_id,
                           const ScriptVariable &receiver,
                           const ScriptVector &dict,
                           ScriptVector &command, ScriptVariable &input) const
{
    ScriptVariable subject =
        inifile->GetModifiedTextParameter("servicemail", 0, "subject",
                                          serv_id.c_str(), "");

    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("receiver", receiver));
    int i;
    for(i = 0; i < dict.Length(); i+=2)
        sub_sub.AddMacro(new ScriptMacroConst(dict[i], dict[i+1]));

    subject = sub_sub(subject);
    sub_sub.AddMacro(new ScriptMacroConst("subject", subject));

    ScriptVariable tmp;

    tmp = inifile->GetModifiedTextParameter("servicemail", 0, "send_command",
                                            serv_id.c_str(), "");
    bool ok = make_argv(sub_sub(tmp), command);
    if(!ok || command.Length() < 1) {
        command.Clear();
        input.Invalidate();
        return;
    }

    tmp = inifile->GetModifiedTextParameter("servicemail", 0, "header",
                                            serv_id.c_str(), "");
    tmp.Trim();
    if(tmp == "") {
        input.Invalidate();
        return;
    }
    ScriptVariable header = sub_sub(tmp);

    tmp = inifile->GetModifiedTextParameter("servicemail", 0, "body",
                                            serv_id.c_str(), "");
    tmp.Trim();
    if(tmp == "") {
        input.Invalidate();
        return;
    }
    input = header + "\n" + sub_sub(tmp);
}


static const char default_error_page_template[] =
    "<?xml version=\"1.0\" encoding=\"US-ASCII\" ?>\n"
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n"
    "  \"http://www.w3.org/TR/xhtml1/DTD/strict.dtd\">\n"
    "<html xmlns=\"http://www.w3.org/TR/xhtml1/strict\" >\n"
    "<head><title>Thalassa CMS default error page</title></head>\n"
    "<body><h1>Thalassa CMS default error page</h1>\n"
    "<h2>%errcode% %errmessage%</h2>\n"
    "<p>If you see this page it probably means the configuration\n"
    "file for Thalassa CMS CGI program is either missing or\n"
    "broken.  Please check.</p></body></html>\n";


ScriptVariable
ThalassaCgiDb::MakeErrorPage(int code, const ScriptVariable &comment) const
{
    ScriptVariable templ =
        inifile->GetTextParameter("errorpage", 0, "template",
                                  default_error_page_template);
    ScriptNumber code_s(code);
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("errcode", code_s));
    sub_sub.AddMacro(new ScriptMacroConst("errmessage", comment));
    return sub_sub.Process(templ);
}

long ThalassaCgiDb::GeneralPostLimit() const
{
    return inifile->
        GetIntegerParameter("general", 0, "post_content_limit", 16);
}

//#include <stdio.h>

int ThalassaCgiDb::FindPath(const ScriptVariable &path, PathData &data) const
{
    const char *tmp;
    const char *pgn = path.c_str();

    bool multipath;

    tmp = inifile->GetTextParameter("page", pgn, "template", 0);
    if(tmp) {
        data.args.Clear();
        multipath = false;
    } else {
        data.args = ScriptWordVector(path, "/");
            // note that it's word vector, not token v., so there can't be
            // empty tokens AND leading and trailing slashes are removed
        if(data.args.Length() < 1)
            return path_bad;
        pgn = data.args[0].c_str();
        tmp = inifile->GetTextParameter("page", pgn, "template", 0);
        if(!tmp)
            return path_noent;
        multipath = true;
    }

    data.page_id = pgn;  // NB: may differ from path!

    tmp = inifile->GetTextParameter("page", pgn, "check_fnsafe", 0);
    if(tmp) {
        ScriptVector tokens;
        bool ok = make_argv(subst->Process(tmp, data.args), tokens);
        if(!ok) {
            //fprintf(stderr, "FAILED make_argv: %s\n", tmp);
            return path_invalid;
        }
        int i;
        for(i = 0; i < tokens.Length(); i++)
             if(!check_fname_safe(tokens[i].c_str())) {
                 //fprintf(stderr, "FAILED check: %s\n", tokens[i].c_str());
                 return path_invalid;
             }
        //fprintf(stderr, "CHECK_FNSAFE OK\n");
    }

    tmp = inifile->GetTextParameter("page", pgn, "session_required", 0);
    data.session_required = boolean_value_from_string(tmp);
    tmp = inifile->GetTextParameter("page", pgn, "embedded", 0);
    data.embedded = boolean_value_from_string(tmp);
    tmp = inifile->GetTextParameter("page", pgn, "post_allowed", 0);
    data.post_allowed = boolean_value_from_string(tmp);

    if(data.post_allowed) {
        data.post_content_limit =
            inifile->GetIntegerParameter("page", pgn, "post_content_limit", -1);
        if(data.post_content_limit == -1)
            data.post_content_limit = inifile->
                GetIntegerParameter("general", 0, "post_content_limit", -1);
        data.post_param_limit =
            inifile->GetIntegerParameter("page", pgn, "post_param_limit", -1);
        if(data.post_param_limit == -1)
            data.post_param_limit = inifile->
                GetIntegerParameter("general", 0, "post_param_limit", -1);
    }

    tmp = inifile->GetTextParameter("page", pgn, "reqargs", 0);
    //fprintf(stderr, "REQARGS: %s\n", tmp);
    if(tmp) {
        ScriptWordVector reqargs(tmp);
        int i;
        for(i = 0; i < reqargs.Length(); i++) {
            const char *ra = reqargs[i].c_str();
            tmp = inifile->
                GetModifiedTextParameter("page", pgn, "reqarg", ra, 0);
            if(!tmp)
                continue;
            subst->SetReqArg(reqargs[i], subst->Process(tmp, data.args));
        }
    }

    if(multipath) {
        tmp = inifile->GetTextParameter("page", pgn, "path_predicate", "yes");
        //fprintf(stderr, "PATH_PREDICATE: %s\n", tmp);
        ScriptVariable r = subst->Process(tmp, data.args);
        r.Trim();
        r.Tolower();
        //fprintf(stderr, "RESULT: %s\n", r.c_str());
        if(r == "reject")
            return path_noaccess;
        if(r != "yes")
            return path_notfound;
    }

    return path_ok;
}

ScriptVariable ThalassaCgiDb::BuildPage(PathData &data) const
{
    return BuildResultPage(data, 0, true);
}

ScriptVariable ThalassaCgiDb::BuildResultPage(PathData &data,
                              const char *msgid, bool is_it_ok) const
{
    subst->SetPageData(&data);
    if(msgid && *msgid) {
        ScriptVariable message;
        const char *tmp = inifile->GetTextParameter("message", 0, msgid, 0);
        if(tmp)
            message = (*subst)(tmp);
        else
            message = ScriptVariable("[==") + msgid + "==]";
        bool requested_action_result =
            ScriptVariable(msgid) != "cookie_set";
        subst->SetMessage(message, is_it_ok, requested_action_result);
    }

    const char *tmp;
    const char *pgid = data.page_id.c_str();

    tmp = inifile->GetTextParameter("page", pgid, "selector", 0);
    if(tmp) {
        data.selector = subst->Process(tmp, data.args);
        data.selector.Trim();
    } else {
        data.selector = ScriptVariableInv();
    }

    tmp = inifile->GetTextParameter("page", pgid, "template", 0);
    ScriptVariable res = subst->Process(tmp, data.args);
    subst->ForgetMessage();
    subst->ForgetPageData();
    return res;
}


static bool is_alphanumeric(int c)
{
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_';
}

bool ThalassaCgiDb::GetPageAction(const PathData &data,
                                  ScriptVector &action) const
{
    const char *pgn = data.page_id.c_str();
    const char *tmp = inifile->GetTextParameter("page", pgn, "action", 0);
    if(!tmp)
        return false;
    ScriptVariable actstr(tmp);
    actstr.Trim();
    if(actstr.Length() == 0)
        return false;
    if(is_alphanumeric(actstr[0])) {   // just words, then
        action = ScriptWordVector(actstr);
    } else {                // otherwise, use it as a delimiter for tokens
        char dlm[2] = { 0, 0 };
        dlm[0] = actstr[0];
        actstr.Range(0, 1).Erase();
        action = ScriptTokenVector(actstr, dlm, " \t\r\n");
    }
    if(action.Length() == 0)
        return false;
    subst->SetPageData(&data);
    int i;
    for(i = 0; i < action.Length(); i++)
        action[i] = subst->Process(action[i], data.args);
    subst->ForgetPageData();
    return true;
}

ScriptVariable ThalassaCgiDb::GetPageProperty(const PathData &data,
                                   const ScriptVariable &prop_id) const
{
    const char *pgn = data.page_id.c_str();
    const char *prid = prop_id.c_str();
    const char *tmp = data.selector.IsValid() ?
        inifile->GetModifiedTextParameter("page", pgn, prid,
                                  data.selector.c_str(), "") :
        inifile->GetTextParameter("page", pgn, prid, "");
    return subst->Process(tmp, data.args);
    //return ScriptVariable("--[") + data.page_id + "|" + prop_id + "]--";
}


ScriptVariable ThalassaCgiDb::MakeNocookiePage() const
{
    ScriptVariable templ =
        inifile->GetTextParameter("nocookiepage", 0, "template", "");
    return subst->Process(templ);
}

ScriptVariable ThalassaCgiDb::MakeRetryCaptchaPage(int captcha_result) const
{
    ScriptVariable templ =
        inifile->GetTextParameter("retrycaptchapage", 0, "template", "");

    const char *modif;
    switch(captcha_result) {
    case captcha_result_ip_mismatch: modif = "ip_mismatch"; break;
    case captcha_result_expired:     modif = "expired"; break;
    case captcha_result_broken_data: modif = "broken_data"; break;
    case captcha_result_wrong:       modif = "wrong_answer"; break;
    default:                         modif = "unknown"; break;
    }

    const char *msg =
        inifile->GetModifiedTextParameter("retrycaptchapage", 0, "errmessage",
                                          modif, "captcha problem");
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("errmessage", msg));
    return sub_sub.Process(templ);
}

ScriptVariable ThalassaCgiDb::GetCommentDir() const
{
    const char *tmp;
    tmp = inifile->GetTextParameter("comments", 0, "dir", ".");
    return (*subst)(tmp);
}

bool ThalassaCgiDb::GetDiscussionInfo(const ScriptVariable &comment,
                                      DiscussionInfo &result) const
{
    ScriptMacroprocessor sub_sub(subst);
    sub_sub.AddMacro(new ScriptMacroConst("comment_id", comment));

    if(comment.IsInvalid() || comment == "") {
        result.comment_id = 0;
    } else {
        long n;
        bool ok = comment.GetLong(n, 10);
        if(!ok)
            return false;
        result.comment_id = n;
    }

    const char *tmp;
    ScriptVariableInv inv;

    tmp = inifile->GetTextParameter("comments", 0, "subdir", 0);
    if(!tmp)
        return false;
    result.cmt_tree_dir = GetCommentDir() + "/" + (*subst)(tmp).Trim();

    tmp = inifile->GetTextParameter("comments", 0, "page_source", 0);
    result.page_source = tmp ? (*subst)(tmp).Trim() : inv;

    tmp = inifile->GetTextParameter("comments", 0, "page_html_file", 0);
    result.page_html_file = tmp ? (*subst)(tmp).Trim() : inv;

    tmp = inifile->GetTextParameter("comments", 0, "page_url", 0);
    result.page_url = tmp ? (*subst)(tmp).Trim() : inv;

    tmp = inifile->GetTextParameter("comments", 0, "orig_url", 0);
    result.orig_url = tmp ? sub_sub(tmp).Trim() : inv;
                                         // NB: the only use of sub_sub!
    result.html_start_mark.Invalidate();
    result.html_end_mark.Invalidate();

    tmp = inifile->GetTextParameter("comments", 0, "page_html_marks", 0);
    if(tmp) {
        ScriptTokenVector v(tmp, "\n", " \t\r");
        int vl = v.Length();
        int i = 0;
        while(i < vl && v[i] == "")
            i++;
        if(i < vl)
            result.html_start_mark = v[i];
        if(i+1 < vl)
            result.html_end_mark = v[i+1];
    }

    tmp = inifile->GetTextParameter("comments", 0, "access", 0);
    result.access = tmp ? (*subst)(tmp) : inv;

    tmp = inifile->GetTextParameter("comments", 0, "premodq_page_id", 0);
    result.premodq_page_id = tmp ? (*subst)(tmp).Trim() : inv;

    result.recent_timeout =
        inifile->GetIntegerParameter("comments", 0, "recent_timeout", 0);
    result.recent_timeout *= 60;   /* it is in minutes */

    return true;
}

static bool any_whitespace(const char *s)
{
    const char *p;
    for(p = s; *p; p++)
        if(*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
            return true;
    return false;
}

static bool scan_subdir(const ScriptVariable &path, ScriptVector &res)
{
    bool have_subdirs = false;
    res.Clear();
    ReadDir rd(path.c_str());
    const char *nm;
    while((nm = rd.Next())) {
        if(*nm == '.' || *nm == '_' || any_whitespace(nm))
            continue;
        ScriptVariable fname = path + "/" + nm;
        FileStat fs(fname.c_str());
        if(!fs.IsDir())
            continue;
        have_subdirs = true;

        ScriptVector sub_subs;
        bool sub_has_subs = scan_subdir(fname, sub_subs);
        if(!sub_has_subs) {
            res.AddItem(nm);
            continue;
        }

        // it's a subdir with subdirs!  (the most messy case)
        ScriptVariable basename(nm);
        basename += "/";
        int i;
        for(i = 0; i < sub_subs.Length(); i++)
            res.AddItem(basename + sub_subs[i]);
    }
    return have_subdirs;
}

void ThalassaCgiDb::GetCommentTopics(ScriptVector &topics) const
{
    ScriptVariable cdr = GetCommentDir();
    scan_subdir(cdr, topics);
}

bool ThalassaCgiDb::CommentTopicExists(const ScriptVariable &topic) const
{
    ScriptVariable cdr = GetCommentDir();
    ScriptVariable fname = cdr + "/" + topic;
    FileStat fs(fname.c_str());
    if(!fs.Exists() || !fs.IsDir())
        return false;
    ScriptVector v;
    bool has_subdirs = scan_subdir(fname, v);
    return !has_subdirs;
}

bool ThalassaCgiDb::MakeCommentPageRegenCmd(ScriptVector &result) const
{
    const char *tmp;
    tmp = inifile->GetTextParameter("comments", 0, "page_regen_command", 0);
    if(!tmp)
        return false;
    bool ok = make_argv((*subst)(tmp), result);
    if(!ok || result.Length() < 1) {
        result.Clear();
        return false;
    }
    return true;
}

bool ThalassaCgiDb::CanPost(bool &bypass_premod) const
{
    if(!the_session)   // this is actually a bug!
        return false;
    EnsureAccessChecker();
    ScriptVector roles;
    the_session->GetCurrentRoles(roles);
    return access_checker->CanPost(roles, bypass_premod);
}

bool ThalassaCgiDb::CanSeeHidden(const ScriptVariable &comment_owner_id) const
{
    if(!the_session)   // this is actually a bug!
        return false;
    EnsureAccessChecker();
    ScriptVector roles;
    the_session->GetCurrentRoles(roles);
    bool own =
        the_session->IsLoggedIn() &&
        the_session->GetUser() == comment_owner_id;
    return access_checker->CanSeeHidden(roles, own);
}

bool ThalassaCgiDb::CanModerate() const
{
    if(!the_session)   // this is actually a bug!
        return false;
    EnsureAccessChecker();
    ScriptVector roles;
    the_session->GetCurrentRoles(roles);
    return access_checker->CanModerate(roles);
}

bool ThalassaCgiDb::CanEdit(const ScriptVariable &comment_owner_id,
                            long long comment_unixdate) const
{
    if(!the_session)   // this is actually a bug!
        return false;
    EnsureAccessChecker();
    ScriptVector roles;
    the_session->GetCurrentRoles(roles);
    bool own =
        the_session->IsLoggedIn() &&
        the_session->GetUser() == comment_owner_id;
    return access_checker->CanEdit(roles, own, comment_unixdate);
}

void ThalassaCgiDb::EnsureAccessChecker() const
{
    if(access_checker)
        return;

    const char *rules =
        inifile->GetTextParameter("comments", 0, "access", "");
    int recent =
        inifile->GetIntegerParameter("comments", 0, "recent_timeout", 0);

    if(!access_checker)
        const_cast<ThalassaCgiDb*>(this)->access_checker =
            new AccessChecker();
    access_checker->SetRules(rules, recent * 60);
}

void ThalassaCgiDb::SetJustPostedSubst(const ScriptVariable &comment,
                                       bool hidden)
{
    subst->SetJustPosted(comment, hidden);
}

void ThalassaCgiDb::CurrentCommentChanged(const ScriptVariable &c) const
{
    const char *tmp =
        inifile->GetTextParameter("comments", 0, "premodq_page_id", "");
    ScriptVariable pg = tmp ? (*subst)(tmp) : ScriptVariableInv();
    subst->CurrentCommentChanged(pg, c);
}

void ThalassaCgiDb::SetPreview(const NewCommentData &ncd)
{
    subst->SetPreview(ncd);
}

