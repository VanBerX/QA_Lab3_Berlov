#include <stdlib.h>   // for getenv
#include <time.h>

#include <scriptpp/scrmsg.hpp>

#include "fnchecks.h"
#include "qsrt.h"
#include "basesubs.hpp"
#include "tcgi_db.hpp"
#include "tcgi_ses.hpp"
#include "tcgi_rpl.hpp"
#include "xcgi.hpp"
#include "xcaptcha.hpp"

#include "tcgi_sub.hpp"



#include <stdio.h>  // debugging only, must disappear one day

///////////////////////////////////////////////////////////////////////////

class VarHtmlSection : public ScriptMacroprocessorMacro {
    const ThalassaCgiDbSubstitution *the_master;
    const ThalassaCgiDb *the_database;
public:
    VarHtmlSection(const ThalassaCgiDbSubstitution *m, const ThalassaCgiDb *db)
        : ScriptMacroprocessorMacro("html"), the_master(m), the_database(db)
    {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarHtmlSection::Expand(const ScriptVector &params) const
{
    int len = params.Length();
    if(len < 1)
        return ScriptVariableInv();
    ScriptVariable sn = the_database->GetHtmlSnippet(params[0].Trim());
    return the_master->Process(sn, params, 1, len-1);
}

///////////////////////////////////////////////////////////////////////////

#if 0
class VarRequestData : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
public:
    VarRequestData(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("req", true), the_database(m) {}
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
    Cgi *req = the_database->GetRequest();
    if(!req || params.Length() < 1)
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
#endif


///////////////////////////////////////////////////////////////////////////

class VarSessionData : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
    ScriptVariable pqprev, pqcurr, pqnext;
public:
    VarSessionData(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("sess"), the_database(m) {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void ForceGetQueuePosition(const ScriptVariable &page,
                               const ScriptVariable &comment);
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
    void LoadPQ(SessionData *sess, ScriptVariable pgid, ScriptVariable cmtid);
};

ScriptVariable VarSessionData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarSessionData::DoExpand(const ScriptVector &params) const
{
    SessionData *sess = the_database->GetSession();
    if(!sess || params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "cookie")
        return sess->GetId();
    if(s == "ifvalid")
        return sess->IsValid() ? params[1] : params[2];
    if(s == "ifloggedin")
        return sess->IsLoggedIn() ? params[1] : params[2];
    if(s == "ifhasuser")
        return sess->HasUser() ? params[1] : params[2];
    if(s == "user")
        return sess->GetUser();
    if(s == "loggeduser")
        return sess->IsLoggedIn() ? sess->GetUser() : ScriptVariable("");
    if(s == "username")
        return sess->GetUserRealname();
    if(s == "useremail")
        return sess->GetUserEmail();
    if(s == "usersite")
        return sess->GetUserSite();
    if(s == "ifcanchangemail")
        return sess->CanRequestEmailChange() ? params[1] : params[2];
    if(s == "ifchangemail")
        return sess->IsChangingEmail() ? params[1] : params[2];
    if(s == "usernewemail")
        return sess->GetUserNewEmail();
    if(s == "roles") {
        ScriptVector r;
        sess->GetCurrentRoles(r);
        return r.Join(" ");
    }
    if(s == "premodq") {
        ScriptVector r;
        sess->GetPremodQueue(r);
        return r.Join(" ");
    }
    if(s == "pqprev") {
        const_cast<VarSessionData*>(this)->
            LoadPQ(sess, params[1], params[2]);
        return pqprev;
    }
    if(s == "pqnext") {
        const_cast<VarSessionData*>(this)->
            LoadPQ(sess, params[1], params[2]);
        return pqnext;
    }
    return ScriptVariable("[sess:") + s + "?!]";
}

void VarSessionData::ForceGetQueuePosition(const ScriptVariable &page,
                                           const ScriptVariable &comment)
{
    SessionData *sess = the_database->GetSession();
    LoadPQ(sess, page, comment);
}

void VarSessionData::LoadPQ(SessionData *sess,
                            ScriptVariable pgid,
                            ScriptVariable cmtid)
{
    pgid.Trim();
    cmtid.Trim();
    ScriptVariable x = pgid + "=" + cmtid;
    if(x == pqcurr)
        return;
    pqcurr = x;
    ScriptVector que;
    sess->GetPremodQueue(que);
    int qln = que.Length();
    int i;
    for(i = 0; i < qln; i++)
        if(que[i] == x)
            break;
    if(i >= qln)
        return;
    pqprev = (i >= 1) ? que[i-1] : ScriptVariableInv();
    pqnext = (i < qln-1) ? que[i+1] : ScriptVariableInv();
}


///////////////////////////////////////////////////////////////////////////

class VarProfileData : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
    ScriptVariable username;
    ConfigInformation userinfo;
    bool read_success;
public:
    VarProfileData(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("profile"), the_database(m) {}
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
};

ScriptVariable VarProfileData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarProfileData::DoExpand(const ScriptVector &params) const
{
    SessionData *sess = the_database->GetSession();
    if(!sess || params.Length() < 2)
        return ScriptVariableInv();
    ScriptVariable name = params[0];
    name.Trim().Tolower();
    if(username.IsInvalid() || username != name) {
        VarProfileData *th = const_cast<VarProfileData*>(this);
        th->read_success = sess->GetProfile(name, th->userinfo);
        th->username = name;
    }
    ScriptVariable s = params[1];
    s.Trim();
    if(s == "ifexists")
        return read_success ? params[2] : params[3];
    if(s == "id")
        return username;
    return userinfo.GetItem(s);
}


///////////////////////////////////////////////////////////////////////////

class VarPageData : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
    const PathData *the_pd;
public:
    VarPageData(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("page"), the_database(m), the_pd(0) {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void SetData(const PathData *pd) { the_pd = pd; }
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
};

ScriptVariable VarPageData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarPageData::DoExpand(const ScriptVector &params) const
{
    if(!the_pd || params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "ifembedded")
        return the_pd->embedded ? params[1] : params[2];
    if(s == "ifpostallowed")
        return the_pd->post_allowed ? params[1] : params[2];
    if(s == "ifsessionrequired")
        return the_pd->session_required ? params[1] : params[2];
    return the_database->GetPageProperty(*the_pd, s);
    //return ScriptVariable("[page:") + s + "?!]";
}

///////////////////////////////////////////////////////////////////////////

class VarCommentMap : public ScriptMacroprocessorMacro {
public:
    VarCommentMap() : ScriptMacroprocessorMacro("commentmap") {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable VarCommentMap::Expand(const ScriptVector &params) const
{
    //   %[commentmap:path/to/file:cmtid:default_value]

    ScriptVariable mfn = params[0];
    mfn.Trim();
    ScriptVariable dfv = params[2];
    if(dfv.IsValid())
        dfv.Trim();
    if(mfn == "")         // no map file name, just return the default
        return dfv;
    ScriptVariable cid = params[1];
    if(cid.IsInvalid())
        cid = "";
    else
        cid.Trim();
    if(cid == "")   // no comment id
        return dfv;
    ReadStream f;
    if(!f.FOpen(mfn.c_str()))
        return dfv;
    ScriptVector v;
    while(f.ReadLine(v, 2, " \t\r")) {
        if(v[0].Trim() == cid)
            return v[1].Trim();
    }
    return dfv;
}


///////////////////////////////////////////////////////////////////////////

class VarFeedbackData : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
public:
    VarFeedbackData(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("feedback"), the_database(m) {}
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
};

ScriptVariable VarFeedbackData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarFeedbackData::DoExpand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "categories")
        return the_database->GetContactCategories();
    if(s == "cattitle")
        return the_database->GetContactCatTitle(params[1]);
    if(s == "ifcatsel")
        return the_database->GetContactCatIsSel(params[1]) ?
            params[2] : params[3];
    if(s == "envfrom")
        return the_database->GetContactEnvelopeFrom();
    return ScriptVariable("[feedback:") + s + "?!]";
}

///////////////////////////////////////////////////////////////////////////

class VarCaptchaData : public ScriptMacroprocessorMacro {
public:
    VarCaptchaData() : ScriptMacroprocessorMacro("captcha") {}
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
};

ScriptVariable VarCaptchaData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarCaptchaData::DoExpand(const ScriptVector &params) const
{
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "image")
        return captcha_image_base64();
    if(s == "ip")
        return captcha_ip();
    if(s == "time")
        return captcha_time();
    if(s == "nonce")
        return captcha_nonce();
    if(s == "token")
        return captcha_token();
    return ScriptVariable("[captcha:") + s + "?!]";
}



///////////////////////////////////////////////////////////////////////////

class VarReqArg : public ScriptMacroprocessorMacro {
    ScriptVector dict;
public:
    VarReqArg() : ScriptMacroprocessorMacro("reqarg") {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void SetArg(const ScriptVariable &key, const ScriptVariable &val) {
        dict.AddItem(key);
        dict.AddItem(val);
    }
};

ScriptVariable VarReqArg::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1 || params[0].IsInvalid())
        return ScriptVariableInv();
    params[0].Trim();
    int i;
    for(i = 0; i < dict.Length()-1; i++)
        if(params[0] == dict[i])
            return dict[i+1];
    return "";
}

///////////////////////////////////////////////////////////////////////////

class SplitPremodQ : public ScriptMacroprocessorMacro {
    ScriptMacroprocessor *the_master;
public:
    SplitPremodQ(ScriptMacroprocessor *m)
        : ScriptMacroprocessorMacro("splitpremodq"), the_master(m) {}
    ScriptVariable Expand(const ScriptVector &params) const;
};

ScriptVariable SplitPremodQ::Expand(const ScriptVector &params) const
{
    // %[splitpremodq:func:param1:..:paramN:[...=]pgid=cmtid]

    int paramlen = params.Length();
    if(paramlen < 2)
        return ScriptVariableInv();

    int i;
    ScriptVector args;
    for(i = 1; i < paramlen-1; i++)
        args.AddItem(params[i].Trim());

    ScriptVector v(params[paramlen-1], "=", " \t\r\n");
    for(i = 0; i < v.Length(); i++)
        args.AddItem(v[i]);

    return the_master->Apply(params[0].Trim(), args);
}

///////////////////////////////////////////////////////////////////////////

class VarDiscussData : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
    ScriptVariable comment_id;
    DiscussionInfo discuss_info;
    DiscussDisplayData data;
    bool data_found;
public:
    VarDiscussData(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("discuss"), the_database(m),
        data_found(false) {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void Reset();
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
    void GetDataIfNecessary(ScriptVariable comment);
};

ScriptVariable VarDiscussData::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarDiscussData::DoExpand(const ScriptVector &params) const
{
    if(params.Length() < 2)
        return ScriptVariableInv();
    const_cast<VarDiscussData*>(this)->GetDataIfNecessary(params[1]);
    ScriptVariable s = params[0];
    s.Trim();

    if(s == "iffound")
        return data_found ?  params[2] : params[3];
    if(s == "ifenabled")
        return data.comments_enabled ? params[2] : params[3];
    if(s == "ifhidden")
        return data.hidden ? params[2] : params[3];
    if(s == "ifanon") {
        bool an = (data.user_id.IsInvalid() || data.user_id == "");
        return an ? params[2] : params[3];
    }
    if(s == "ifparent")
        return (data.parent_id > 0) ? params[2] : params[3];
    if(s == "parent")
        return data.parent_id >= 0 ? ScriptNumber(data.parent_id) :
                                     ScriptVariable("");
    if(s == "title")
        return data.title;
    if(s == "body")
        return data.body;
    if(s == "bodysrc")
        return data.bodysrc;
    if(s == "user")
        return data.user_id;
    if(s == "unixtime")
        return ScriptNumber(data.unixtime);
    if(s == "username")
        return data.user_name;
    if(s == "flags")
        return data.flags.Join(" ");
    if(s == "page_url")
        return discuss_info.page_url;
    if(s == "orig_url")
        return discuss_info.orig_url;
    return ScriptVariable("[discuss:") + s + "?!]";
}

void VarDiscussData::GetDataIfNecessary(ScriptVariable comment)
{
    fprintf(stderr, "GETDATAIFNECESSARY\n");
    comment.Trim();
    if(comment == comment_id && data_found)
        return;
    comment_id = comment;
    if(comment.IsValid())
        fprintf(stderr, "COMMENT_ID: [%s]\n", comment.c_str());
    if(comment.IsValid() && comment != "" &&
        !check_fname_safe(comment.c_str()))
    {
        data_found = false;
        return;
    }
    bool ok;
    ok = the_database->GetDiscussionInfo(comment, discuss_info);
    if(!ok) {
        fprintf(stderr, "GETDISCUSSIONINFO FAILED\n");
        data_found = false;
        return;
    }

    fprintf(stderr, "DI:comment_id=%d\n", discuss_info.comment_id);
    fprintf(stderr, "DI:cmt_tree_dir=%s\n", discuss_info.cmt_tree_dir.c_str());
    fprintf(stderr, "DI:page_source=%s\n", discuss_info.page_source.c_str());
    fprintf(stderr, "DI:page_html_file=%s\n",
                                       discuss_info.page_html_file.c_str());
    fprintf(stderr, "DI:page_url=%s\n", discuss_info.page_url.c_str());
    fprintf(stderr, "DI:orig_url=%s\n", discuss_info.orig_url.c_str());

    const char *encoding;
    const char *tags;
    const char *attrs;
    the_database->GetFormatData(encoding, tags, attrs);
    ok = get_discuss_display_data(encoding, tags, attrs, discuss_info, data);
    if(!ok) {
        fprintf(stderr, "GET_DISCUSS_DISPLAY_DATA FAILED\n");
    }
    data_found = ok;
}

void VarDiscussData::Reset()
{
    comment_id = "";
    data_found = false;
    data.body = "RESET!";
    data.title = "RESET!";
}


///////////////////////////////////////////////////////////////////////////

class VarCommentInfo : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
    ScriptVariable subdir;
    int comment_id;
    HeadedTextMessage *msg;
    ScriptVariable title, username;
public:
    VarCommentInfo(const ThalassaCgiDb *m);
    ~VarCommentInfo();
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
    void GetDataIfNecessary(ScriptVariable page, int comment);
};

VarCommentInfo::VarCommentInfo(const ThalassaCgiDb *m)
    : ScriptMacroprocessorMacro("cmtinfo"), the_database(m), msg(0)
{
}

VarCommentInfo::~VarCommentInfo()
{
    if(msg)
        delete msg;
}

ScriptVariable VarCommentInfo::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarCommentInfo::DoExpand(const ScriptVector &params) const
{
    if(params.Length() < 1)
        return ScriptVariableInv();

    ScriptVariable s = params[0];
    s.Trim();

    if(s == "tags") {
        const char *encd;
        const char *tags;
        const char *attrs;
        the_database->GetFormatData(encd, tags, attrs);
        return tags;
    }

    if(s == "attrs") {
        const char *encd;
        const char *tags;
        const char *attrs;
        the_database->GetFormatData(encd, tags, attrs);
        return attrs;
    }

    if(s == "topics") {
        ScriptVector res;
        the_database->GetCommentTopics(res);
        return res.Join(" ");
    }

    if(s == "iftopic") {
        if(params.Length() < 3)
            return ScriptVariableInv();
        ScriptVariable topic = params[1];
        topic.Trim();
        return params[the_database->CommentTopicExists(topic) ?  2 : 3];
    }

    if(params.Length() < 2)
        return ScriptVariableInv();

    if(s == "list") {
        ScriptVariable dir = the_database->GetCommentDir();
        ScriptVector v;
        bool ok = get_comment_list(dir, params[1], v);
        if(!ok)
            return ScriptVariableInv();
        int vl = v.Length();
        int *nums = new int[vl];
        int i;
        for(i = 0; i < vl; i++) {
            long n;
            ok = v[i].GetLong(n, 10);
            nums[i] = ok ? n : -1;
        }
        quicksort_int(nums, vl);
        ScriptVariable res;
        for(i = 0; i < vl; i++)
            if(nums[i] > 0)
                res += ScriptNumber(nums[i]) + " ";
        delete [] nums;
        return res;
    }

    if(params.Length() < 3)
        return ScriptVariableInv();
    long cn;
    bool ok = params[2].GetLong(cn, 10);
    if(!ok)
        return ScriptVariableInv();
    const_cast<VarCommentInfo*>(this)->GetDataIfNecessary(params[1], cn);

    if(s == "iffound")
        return msg ? params[3] : params[4];

    if(!msg)
        return ScriptVariableInv();

    if(s == "ifhidden")
        return htm_has_flag(*msg, "hidden") ? params[3] : params[4];
    if(s == "ifanon")
        return htm_has_flag(*msg, "anon") ? params[3] : params[4];
    if(s == "ifparent") {
        ScriptVariable p = msg->FindHeader("parent");
        bool nopar = p.IsInvalid() || p.Trim() == "" || p == "0";
        return !nopar ? params[3] : params[4];
    }
    if(s == "user" || s == "unixtime" || s == "parent")
        return msg->FindHeader(s);
    if(s == "title")
        return title;
    if(s == "username")
        return username;
    if(s == "flags") {
        ScriptVariable v = msg->FindHeader("flags");
        if(v.IsInvalid() || v.Trim() == "")
            return ScriptVariableInv();
        return ScriptVector(v, ", \t\r\n").Join(" ");
    }

    return ScriptVariable("[cmtinfo:") + s + "?!]";
}

void VarCommentInfo::GetDataIfNecessary(ScriptVariable page, int comment)
{
    if(page == subdir && comment == comment_id && msg)
        return;
    if(msg) {
        delete msg;
        msg = 0;
    }
    subdir = page;
    comment_id = comment;
    if(!check_fname_safe(page.c_str()))
        return;

    ScriptVariable dir = the_database->GetCommentDir();
    msg = new HeadedTextMessage;

    bool ok;
    ok = get_comment_hdr_by_path(dir, subdir, comment_id, *msg);
    if(!ok) {
        delete msg;
        msg = 0;
        return;
    }
    const char *encoding;
    const char *tags;
    const char *attrs;
    the_database->GetFormatData(encoding, tags, attrs);
    get_encoded_fields_from_hm(*msg, encoding, tags, attrs, title, username);
}

///////////////////////////////////////////////////////////////////////////

class VarPermissionChecks : public ScriptMacroprocessorMacro {
    const ThalassaCgiDb *the_database;
public:
    VarPermissionChecks(const ThalassaCgiDb *m)
        : ScriptMacroprocessorMacro("ifperm"), the_database(m) {}
    ScriptVariable Expand(const ScriptVector &params) const;
private:
    ScriptVariable DoExpand(const ScriptVector &params) const;
};

ScriptVariable VarPermissionChecks::Expand(const ScriptVector &params) const
{
    ScriptVariable res = DoExpand(params);
    if(res.IsInvalid())
        res = "";
    return res;
}

ScriptVariable VarPermissionChecks::DoExpand(const ScriptVector &params) const
{
    if(params.Length() < 2)
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();

    if(s == "post") {
        bool ignored;
        return the_database->CanPost(ignored) ? params[1] : params[2];
    }
    if(s == "seehidden")
        return the_database->CanSeeHidden(params[1].Trim()) ?
            params[2] : params[3];
    if(s == "moderate" || s == "moderation")
        return the_database->CanModerate() ? params[1] : params[2];
    if(s == "edit") {
        long long ud;
        params[2].Trim();
        bool ok = params[2].GetLongLong(ud, 10);
        if(!ok)
            ud = 0;
        return the_database->CanEdit(params[1].Trim(), ud) ?
            params[3] : params[4];
    }
    return ScriptVariable("[ifperm:") + s + "?!]";
}

///////////////////////////////////////////////////////////////////////////

class VarJustPostedData : public ScriptMacroprocessorMacro {
    bool exists, hidden;
    ScriptVariable comment;
public:
    VarJustPostedData()
        : ScriptMacroprocessorMacro("justposted"), exists(false) {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void Set(const ScriptVariable &c, bool hd);
};

ScriptVariable VarJustPostedData::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1 || params[0].IsInvalid())
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "if" || s == "ifhave")
        return exists ? params[1] : params[2];
    if(s == "ifhidden")
        return hidden ? params[1] : params[2];
    if(s == "comment")
        return comment;
    return ScriptVariable("[justposted:") + s + "?!]";
}

void VarJustPostedData::Set(const ScriptVariable &c, bool hd)
{
    exists = true;
    comment = c;
    hidden = hd;
}

///////////////////////////////////////////////////////////////////////////

class VarPreviewData : public ScriptMacroprocessorMacro {
    bool preview;
    ScriptVariableInv user, username, title, body;
public:
    VarPreviewData()
        : ScriptMacroprocessorMacro("cmtpreview"), preview(false) {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void Set(const char *enc, const char *tags, const char *attrs,
             const NewCommentData &ncd);
};

ScriptVariable VarPreviewData::Expand(const ScriptVector &params) const
{
    if(params.Length() < 1 || params[0].IsInvalid())
        return ScriptVariableInv();
    ScriptVariable s = params[0];
    s.Trim();
    if(s == "if")
        return preview ? params[1] : params[2];

    if(!preview)
        return ScriptVariableInv();

    if(s == "user")
        return user;
    if(s == "username")
        return username;
    if(s == "title")
        return title;
    if(s == "body")
        return body;
    return ScriptVariable("[cmtpreview:") + s + "?!]";
}

void VarPreviewData::Set(const char *enc, const char *tags, const char *attrs,
                         const NewCommentData &ncd)
{
    preview = true;
    user = ncd.user_id;
    if(user.IsInvalid())
        user = "";
    get_preview_data(enc, tags, attrs, ncd, username, title, body);
}


///////////////////////////////////////////////////////////////////////////

ThalassaCgiDbSubstitution::
ThalassaCgiDbSubstitution(const ThalassaCgiDb *m, const Cgi *c)
    : CommonCgiSubstitutions(c), the_database(m), message(ScriptVariableInv())
{
    pd = new VarPageData(m);
    AddMacro(pd);                                         // [page: ]
    sd = new VarSessionData(m);
    AddMacro(sd);                                         // [sess: ]
    AddMacro(new VarHtmlSection(this, m));                // [html: ]
    AddMacro(new VarProfileData(m));                      // [profile: ]
    AddMacro(new VarFeedbackData(m));                     // [feedback: ]
    AddMacro(new VarPermissionChecks(m));                 // [ifperm: ]

    AddMacro(new VarCaptchaData);                         // [captcha: ]
    AddMacro(new VarCommentMap);                          // [commentmap: ]

    AddMacro(new SplitPremodQ(this));                     // [splitpremodq: ]

    ifmessage = new IfCond("ifmessage");                  // [ifmessage: ]
    AddMacro(ifmessage);
    ifmessageok = new IfCond("ifmessageok");              // [ifmessageok: ]
    AddMacro(ifmessageok);
    ifactresult = new IfCond("ifactresult");              // [ifactresult: ]
    AddMacro(ifactresult);
    ifactresultok = new IfCond("ifactresultok");          // [ifactresultok: ]
    AddMacro(ifactresultok);
    AddMacro(new ScriptMacroScrVar("message", &message)); // [message]

    dd = new VarDiscussData(m);
    AddMacro(dd);                                         // [discuss: ]
    AddMacro(new VarCommentInfo(m));                      // [cmtinfo: ]
    jpsd = new VarJustPostedData;
    AddMacro(jpsd);                                       // [justposted: ]
    prevd = new VarPreviewData;
    AddMacro(prevd);                                      // [cmtpreview:]
    rqa = new VarReqArg;
    AddMacro(rqa);                                        // [reqarg: ]

        // please note that ownersip over these 'var' objects is
        // transferred here to the base class object, and IT will delete them
}

ThalassaCgiDbSubstitution::~ThalassaCgiDbSubstitution()
{
    // no deletion here! "vars" will be deleted by the base class destructor,
    // because these objects belong to it!
}

void ThalassaCgiDbSubstitution::SetPageData(const PathData *d)
{
    pd->SetData(d);
}

void ThalassaCgiDbSubstitution::
SetMessage(const ScriptVariable &m, bool ok, bool action_result)
{
    message = m;
    ifmessage->SetCond(m.IsValid() && m != "");
    ifmessageok->SetCond(ok);
    ifactresult->SetCond(action_result);
    ifactresultok->SetCond(action_result && ok);
}

void ThalassaCgiDbSubstitution::
SetJustPosted(const ScriptVariable &comment, bool hidden)
{
    jpsd->Set(comment, hidden);
}

void ThalassaCgiDbSubstitution::
CurrentCommentChanged(const ScriptVariable &page,
                      const ScriptVariable &comment)
{
    dd->Reset();
    sd->ForceGetQueuePosition(page, comment);
}

void ThalassaCgiDbSubstitution::SetPreview(const NewCommentData &ncd)
{
    const char *encoding;
    const char *tags;
    const char *attrs;
    the_database->GetFormatData(encoding, tags, attrs);
    prevd->Set(encoding, tags, attrs, ncd);
}

void ThalassaCgiDbSubstitution::
SetReqArg(const ScriptVariable &key, const ScriptVariable &val)
{
    fprintf(stderr, "REQARG:%s:%s-\n", key.c_str(), val.c_str());
    rqa->SetArg(key, val);
}
