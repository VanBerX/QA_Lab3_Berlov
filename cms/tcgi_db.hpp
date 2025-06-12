#ifndef TCGI_DB_HPP_SENTRY
#define TCGI_DB_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

class Cgi;

struct PathData {
    bool session_required, embedded, post_allowed;
    long post_content_limit, post_param_limit;
                              // in kilobytes! (which means 1024 bytes)
private:
    ScriptVariable page_id;   // the ini file section id, NOT the path!
    ScriptVariable selector;  // computed
    ScriptVector args;
    friend class ThalassaCgiDb;
};

class ThalassaCgiDb {
    class IniFileParser *inifile;
    class ThalassaCgiDbSubstitution *subst;
    ScriptVariable conf_file;
#if 0
    class Cgi *the_request;
#endif
    class SessionData *the_session;
    class AccessChecker *access_checker;
public:
    ThalassaCgiDb(const Cgi *cgi);
    ~ThalassaCgiDb();

    bool Load(const ScriptVariable &filename);
    ScriptVariable MakeErrorMessage() const;

    void GetFormatData(const char *&encd,
                       const char *&allowed_tags,
                       const char *&allowed_attrs) const;

#if 0
        // this is only for the macroprocessor to have access to the req.
    void SetRequest(Cgi *req) { the_request = req; }
    Cgi *GetRequest() const { return the_request; }
#endif

    void SetSession(SessionData *sd) { the_session = sd; }
    SessionData *GetSession() const { return the_session; }

    ScriptVariable GetUserdataDirectory() const;

    void GetCaptchaParameters(ScriptVariable &secret, int &time_to_live) const;

    ScriptVariable GetHtmlSnippet(const ScriptVariable &name) const;

        // !!! MUST work even when there's no config!
    ScriptVariable MakeErrorPage(int code, const ScriptVariable &cmt) const;

        // as of present, this is used for cookie-set request only
        // default is 16 Kb which must be sufficient
    long GeneralPostLimit() const;

    enum {
        path_ok,         // found
        path_bad,        // failed to parse the path
        path_noent,      // not configured
        path_noaccess,   // path_predicate == ``reject''
        path_notfound,   // path_predicate != ``yes'', != ``reject''
        path_invalid     // check_fnsafe failed
    };
    int FindPath(const ScriptVariable &path, PathData &data) const;

    ScriptVariable BuildPage(PathData &data) const;
    ScriptVariable BuildResultPage(PathData &data,
                                   const char *msgid, bool is_it_ok) const;
    bool GetPageAction(const PathData &data, ScriptVector &action) const;
    ScriptVariable GetPageProperty(const PathData &data,
                                   const ScriptVariable &prop_id) const;

    ScriptVariable MakeNocookiePage() const;
    ScriptVariable MakeRetryCaptchaPage(int captcha_result) const;

    ScriptVariable GetContactCategories() const;
    ScriptVariable GetContactCatTitle(const ScriptVariable &catg) const;
    ScriptVariable GetContactRecipient(const ScriptVariable &catg) const;
    bool           GetContactCatIsSel(const ScriptVariable &catg) const;
    ScriptVariable GetContactEnvelopeFrom() const;
    void BuildContactFormMessage(const ScriptVariable &receiver,
                           ScriptVector &command, ScriptVariable &input) const;

    void BuildServiceEmail(const ScriptVariable &serv_id,
                           const ScriptVariable &receiver,
                           const ScriptVector &dictionary,
                           ScriptVector &command, ScriptVariable &input) const;

    ScriptVariable GetCommentDir() const;
    bool GetDiscussionInfo(const ScriptVariable &comment_id,
                           struct DiscussionInfo &result) const;
    void GetCommentTopics(ScriptVector &topics) const;
    bool CommentTopicExists(const ScriptVariable &topic) const;

    bool MakeCommentPageRegenCmd(ScriptVector &result) const;

    bool CanPost(bool &bypass_premod) const;
    bool CanSeeHidden(const ScriptVariable &comment_owner_id) const;
    bool CanModerate() const;
    bool CanEdit(const ScriptVariable &comment_owner_id,
                 long long comment_unixdate) const;

    void SetJustPostedSubst(const ScriptVariable &comment, bool hidden);
    void CurrentCommentChanged(const ScriptVariable &comment) const;

    void SetPreview(const struct NewCommentData &ncd);

private:
    void EnsureAccessChecker() const;
};

#endif
