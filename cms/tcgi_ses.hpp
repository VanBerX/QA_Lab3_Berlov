#ifndef TCGI_SES_HPP_SENTRY
#define TCGI_SES_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/confinfo.hpp>

class ScriptVector;

class SessionData {
    ScriptVariable dirname;
    ScriptVariable sess_fn;
    bool just_created, just_removed;
    ConfigInformation info;
    ScriptVariable username;
    ConfigInformation userinfo;
    bool userinfo_read, logged_in;
    class EmailData *email_data;

    long captcha_ttl;  // XXX to be replaced with a struct of all params
public:
    SessionData(const ScriptVariable &dirpath);
    ~SessionData();

    void SetCaptchaTtl(int ttl) { captcha_ttl = ttl; }  // XXX
        // to be removed in favor of a structure holding all ``constants''

    bool Validate(ScriptVariable sess_id);

    bool Create(int time_to_live);
    bool JustCreated() const { return just_created; }
    bool JustRemoved() const { return just_removed; }
    bool Renew(int time_to_live);
    bool IsValid() const;
    ScriptVariable GetId() const;   // the value for the cookie
    ScriptVariable GetId0() const;  // only the persistent part

    void Remove();

    void PerformCleanup(int time_to_live);

    void GetCurrentRoles(ScriptVector &roles) const;

        // only has effect if session is valid but not logged in
    void SaveUsedRealname(const ScriptVariable &name);

    bool Login(ScriptVariable login, ScriptVariable passtoken);
    bool IsLoggedIn() const { return logged_in; }

    bool HasUser() const { return username.IsValid() && username != ""; }
    bool SetUser(ScriptVariable u);
    ScriptVariable GetUser() const { return username; }
    bool CheckUserStatus(ScriptVariable login, bool allow_pending);
    ScriptVariable GetUserStatus() const;
    ScriptVariable GetUserRealname() const;
    ScriptVariable GetUserEmail() const;
    ScriptVariable GetUserSite() const;
    bool IsChangingEmail() const;
    ScriptVariable GetUserNewEmail() const;
    int GetUserRemainingPasswords() const;
    long long GetUserLastPwdsent() const
        { return GetUserLastEvent("pwdsent"); }

    bool GenerateNewPasswords(ScriptVector &passwds, int count) const;

    bool UpdateUserLastPwdsent() { return UpdateUserLastEvent("pwdsent"); }
    bool UpdateUserLastLogin() { return UpdateUserLastEvent("login"); }
    bool UpdateUserLastSeen() { return UpdateUserLastEvent("seen"); }

    enum {
        cu_success, cu_bad_id, cu_exists, cu_bad_email, cu_email_used,
        cu_email_replaced, cu_email_recently_tried, cu_email_banned,
        cu_change_req_bad_passwd, cu_change_req_too_early,
        cu_conf_error, cu_bug
    };
    int CreateUser(ScriptVariable u, const ScriptVariable &name,
                   const ScriptVariable &email, const ScriptVariable &site,
                   ScriptVariable &confirmation_code);
    bool CanRequestEmailChange() const;
    int EmailChangeRequest(const ScriptVariable &newmail,
                           const ScriptVariable &passtoken,
                           ScriptVariable &confirmation_code);
    bool EmailChangeConfirm(ScriptVariable confirmation_code);
    void EmailChangeCancel();

    bool UpdateUser(const ScriptVariable &name, const ScriptVariable &site);

        // does NOT affect user/login status in any way
    bool GetProfile(ScriptVariable login, ConfigInformation &prof) const;

    void AddToPremodQueue(const ScriptVariable &pgid,
                          int cmtid, const ScriptVariable &cmt_file) const;
    void RemoveFromPremodQueue(const ScriptVariable &pgid, int cmtid);
    int GetPremodQueue(ScriptVector &res) const;
#if 0
    void ForceGetQueuePosition();
#endif

    bool CheckAndStoreNonce(const ScriptVariable &time,
                            const ScriptVariable &nonce);

private:
    ScriptVariable GetPremodQueueDir() const;

        // it is the caller's duty to force the case and check for safety
    bool CheckAndRemovePassword(const ScriptVariable &login_name,
                                const ScriptVariable &passtoken);

    int CheckAndReserveEmail(const ScriptVariable &email,
                             const ScriptVariable &login);

    bool EnsureUserinfoRead() const;
    ScriptVariable GetUserParameter(const char *id) const;
    long long GetUserLastEvent(const char *id) const;
    bool UpdateUserLastEvent(const char *event);

    bool Save(bool creation);
    bool SaveUserinfo();
};

#endif
