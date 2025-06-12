#include <stdio.h>    // for rename
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>


#include "fnchecks.h"
#include "xrandom.h"
#include "fileops.hpp"
#include "memmail.hpp"

#include "tcgi_ses.hpp"

enum {
    sess_filename_len = 16,              // 64-bit number in hex
    pending_time_max = (24*3600),        // 1 day
    tried_email_cooldown = (31*24*3600), // well, a month
    email_change_cooldown = (24*3600)    // 1 day
};

#define SESS_SUBDIR "_sessions"
#define USER_SUBDIR "_users"
#define USER_FILENAME "_data"
#define EMAIL_SUBDIR "_email"
#define NONCE_SUBDIR "_nonce"
#define PREMOD_QUEUE_SUBDIR "_premod_queue"


SessionData::SessionData(const ScriptVariable &dirpath)
    : dirname(dirpath), just_created(false), just_removed(false),
    username(ScriptVariableInv()), userinfo_read(false), logged_in(false),
    email_data(0)
{
}

SessionData::~SessionData()
{
    if(email_data)
        delete email_data;
}

static bool acceptable_sess_id(const char *sess_id)
{
    if(!sess_id || !*sess_id)
        return false;
    int len = 0;
    while(sess_id[len]) {
        char c = sess_id[len];  // just for short name
        if(!((c>='0' && c<='9') || (c>='A' && c<='Z') || c=='_'))
            return false;
        len++;
    }
#if 0     // the length check is performed later in SessionData::Validate
          // actually, this check is wrong here, because all the cookie
          // value is passed to this function, not the filename part
    if(len != sess_filename_len+1)
        return false;
#endif
    return true;
}

static bool make_dir_if_necessary(const char *path)
{
    FileStat fs(path);
    if(fs.Exists() && fs.IsDir())
        return true;
    int r = mkdir(path, 0700);
    return r != -1;
}

bool SessionData::Validate(ScriptVariable sid)
{
    if(!acceptable_sess_id(sid.c_str()))
        return false;
    ScriptVariable::Substring delimpos = sid.Strchr('_');
    if(delimpos.IsInvalid() || delimpos.Index() != sess_filename_len)
        return false;

    ScriptVariable fn = delimpos.Before().Get();
    ScriptVariable token = delimpos.After().Get();

    ScriptVariable fname = dirname + "/" SESS_SUBDIR "/" + fn;
    info.Clear();
    if(!info.FOpen(fname.c_str()))
        return false;
    bool rr = info.RunParser();
    info.FClose();
    if(!rr)
        return false;

    ScriptVariable tk = info.GetItem("token");
    ScriptVariable oldtk = info.GetItem("old_token");
    if(token != tk && token != oldtk)
        return false;

    sess_fn = fn;
    username = info.GetItem("user");
    ScriptVariable lgg = info.GetItem("logged_in");
    lgg.Trim().Tolower();
    logged_in = lgg == "yes";
    if(logged_in)
        UpdateUserLastSeen();
    return true;
}


#if 0
static inline char hexdig(int n)
{
        // YES, THIS IS BUG :-)
        // definitely it should have been n+'A'-10 here,
        // but the effect is aestetically pleasant
    return n < 10 ? n + '0' : n + 'A';
}
#endif

static ScriptVariable uchar2hex(unsigned char *s, int n)
{
    ScriptVariable res;
    int i;
    for(i = 0; i < n; i++) {
#if 0
        res += hexdig((s[i] & 0xf0) >> 4);
        res += hexdig(s[i] & 0x0f);
#endif
        res += 'A' + ((s[i] & 0xf0) >> 4);
        res += 'A' + (s[i] & 0x0f);
    }
    return res;
}

bool SessionData::Create(int time_to_live)
{
    unsigned char id[8];
    unsigned char tok[8];
    fill_random(id, sizeof(id));
    fill_random(tok, sizeof(tok));

    sess_fn = uchar2hex(id, sizeof(id));
    unsigned long now = time(0);
    info.Clear();
    info.AddItem("token", uchar2hex(tok, sizeof(tok)));
    info.AddItem("created", ScriptNumber(now));
    info.AddItem("expire", ScriptNumber(now + time_to_live));
    just_created = true;
    bool ok = Save(true);
    if(ok)
        return true;
    sess_fn = "";
    info.Clear();
    return false;
}

bool SessionData::Renew(int time_to_live)
{
    if(!IsValid())
        return false;
    ScriptVariable oldold = info.GetItem("oldtoken");
    ScriptVariable old = info.GetItem("token");

    unsigned char tok[8];
    fill_random(tok, sizeof(tok));

    info.SetItem("token", uchar2hex(tok, sizeof(tok)));
    info.SetItem("oldtoken", old);
    unsigned long now = time(0);
    info.SetItem("expire", ScriptNumber(now + time_to_live));

    bool ok = Save(false);
    if(ok)
        return true;
    sess_fn = "";
    info.Clear();
    return false;
}

bool SessionData::IsValid() const
{
    return sess_fn.IsValid() && sess_fn != "";
}

ScriptVariable SessionData::GetId0() const
{
    return sess_fn;
}

ScriptVariable SessionData::GetId() const
{
    if(sess_fn.IsValid() && sess_fn != "")
        return sess_fn + '_' + info.GetItem("token");
    return ScriptVariableInv();
}

void SessionData::Remove()
{
    if(sess_fn.IsInvalid() || sess_fn == "")
        return;
    ScriptVariable fname = dirname + "/" SESS_SUBDIR "/" + sess_fn;
    unlink(fname.c_str());
    info.Clear();
    sess_fn.Invalidate();
    just_removed = true;
}

void SessionData::GetCurrentRoles(class ScriptVector &roles) const
{
    roles.Clear();
    if(!IsValid())
        return;
    if(IsLoggedIn()) {
        ScriptVariable rstr = GetUserParameter("roles");
        if(rstr.IsValid())
            roles = ScriptVector(rstr, ",", " \t\r\n");
    }
    roles.AddItem(IsLoggedIn() ? "auth" : "anon");
    roles.AddItem("all");
}

void SessionData::SaveUsedRealname(const ScriptVariable &name)
{
    if(!IsValid() || IsLoggedIn())
        return;
    info.SetItem("realname", name);
    Save(false);
}

ScriptVariable SessionData::GetUserStatus() const
{
    return GetUserParameter("status");
}

ScriptVariable SessionData::GetUserRealname() const
{
    return IsLoggedIn() ?
        GetUserParameter("realname") : info.GetItem("realname");
}

ScriptVariable SessionData::GetUserEmail() const
{
    return GetUserParameter("email");
}

ScriptVariable SessionData::GetUserSite() const
{
    return GetUserParameter("site");
}

bool SessionData::IsChangingEmail() const
{
    ScriptVariable ne = GetUserNewEmail();
    return ne.IsValid() && ne != "";
}

ScriptVariable SessionData::GetUserNewEmail() const
{
    return GetUserParameter("new_email");
}

ScriptVariable SessionData::GetUserParameter(const char *id) const
{
    if(!HasUser())
        return ScriptVariableInv();
    EnsureUserinfoRead();
    return userinfo.GetItem(id);
}

long long SessionData::GetUserLastEvent(const char *ev) const
{
    ScriptVariable evname = ScriptVariable("last_") + ev;
    ScriptVariable s = GetUserParameter(evname.c_str());
    if(s.IsInvalid() || s == "")
        return -1;
    long long res;
    if(!s.GetLongLong(res, 10))
        return -1;
    return res;
}

bool SessionData::CheckUserStatus(ScriptVariable login, bool allow_pending)
{
    username = login;
    ScriptVariable status = GetUserStatus();
    if(status.IsInvalid() ||
        allow_pending ? (status.Trim().Tolower() == "blocked") :
                        (status.Trim().Tolower() != "active"))
    {
        username.Invalidate();
        userinfo.Clear();
        userinfo_read = false;
        logged_in = false;
        return false;
    }
    return true;
}

bool SessionData::Login(ScriptVariable lgn, ScriptVariable pt)
{
    lgn.Tolower();
    if(HasUser() && username != lgn)
        return false;
    pt.Toupper();
    if(!check_username_safe(lgn.c_str()) || !check_passtoken_safe(pt.c_str()))
        return false;

    bool ok = CheckUserStatus(lgn, true);
    if(!ok)
        return false;

    if(GetUserStatus() == "pending") {  // very special case
        ScriptVariable cc = GetUserParameter("confirmation_code");
        cc.Trim().Toupper();
        if(cc != pt)
            return false;
        // success!  now some bookkeeping for the new welcomed user
        userinfo.SetItem("status", "active");
        userinfo.SetItem("confirmation_code", ScriptVariableInv());
        ScriptVariable email = GetUserEmail();
        if(!email_data)
            email_data = new EmailData(dirname + "/" EMAIL_SUBDIR);
        EmailInfo emi;
        emi.status = "active";
        emi.user = username;
        email_data->Set(email, emi);   // the date is updated there
            // both session and user are saved a bit later
    } else {
            // for an active user, just check the password
        bool passwd_ok = CheckAndRemovePassword(lgn, pt);
        if(!passwd_ok)
            return false;
    }

    // okay, successfully passed
    username = lgn;
    logged_in = true;
    unsigned long now = time(0);
    info.SetItem("user", username);
    info.SetItem("logged_in", "yes");
    info.SetItem("login_time", ScriptNumber(now));
    Save(false);
    UpdateUserLastLogin();
    return true;
}

bool SessionData::SetUser(ScriptVariable lgn)
{
    lgn.Tolower();
    if(HasUser() && username != lgn)
        return false;
    if(!check_username_safe(lgn.c_str()))
        return false;

#if 0
    ScriptVariable fname =
        dirname + "/" USER_SUBDIR "/" + lgn + "/" USER_FILENAME;
    FileStat fs(fname.c_str());
    if(!fs.Exists() || !fs.IsRegularFile())
        return false;
#endif

    bool ok = CheckUserStatus(lgn, true);
    if(!ok)
        return false;

    info.SetItem("user", username);
    info.SetItem("logged_in", logged_in ? "yes" : "no");
    Save(false);
    return true;
}

int SessionData::GetUserRemainingPasswords() const
{
    if(!HasUser())
        return -1;
    ScriptVariable passdir = dirname + "/" USER_SUBDIR "/" + username;

    const char *s;
    ReadDir rdir(passdir.c_str());
    if(!rdir.OpenOk())
        return -1;
    int fcount = 0;
    while((s = rdir.Next())) {
        if(!*s || *s == '.' || *s == '_')
            continue;
        fcount++;
    }
    return fcount;
}

static ScriptVariable make_random_password()
{
    static const char digits[33] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
        // please note this must be exactly 32 chars long
        // we don't use O, I, 0 and 1, so there are 26+10-4 = 32 chars
    long long n;
    fill_random(&n, sizeof(n));
    ScriptVariable res = "";
    int i;
    for(i = 0; i < 8; i++) {
        res += digits[n & 0x1f];
        n >>= 5;
    }
    return res;
}

bool SessionData::GenerateNewPasswords(ScriptVector &pds, int count) const
{
    if(!HasUser())
        return false;
    ScriptVariable passdir = dirname + "/" USER_SUBDIR "/" + username;
    ScriptVariable userdataf = passdir + "/" USER_FILENAME;

    ScriptVector oldpwds;
    const char *s;
    ReadDir rd(passdir.c_str());
    if(!rd.OpenOk())
        return false;
    while((s = rd.Next())) {
        if(!*s || *s == '.' || *s == '_')
            continue;
        oldpwds.AddItem(s);
    }
    int i;
    for(i = 0; i < oldpwds.Length(); i++)
        unlink((passdir + "/" + oldpwds[i]).c_str());

    pds.Clear();
    for(i = 0; i < count; i++) {
        ScriptVariable p = make_random_password();
        ScriptVariable pfname = passdir + "/" + p;
        int res = link(userdataf.c_str(), pfname.c_str());
        if(res == -1)
            continue;
        pds.AddItem(p);
    }
    return pds.Length() > 0;
}

bool SessionData::EnsureUserinfoRead() const
{
    if(!HasUser())
        return false;
    if(userinfo_read)
        return true;

    ConfigInformation *ui = const_cast<ConfigInformation*>(&userinfo);
    bool rr = GetProfile(username, *ui);
    if(!rr)
        return false;

    const_cast<SessionData*>(this)->userinfo_read = true;
    return true;
}

bool SessionData::UpdateUserLastEvent(const char *ev)
{
    if(!HasUser())
       return false;
    EnsureUserinfoRead();

    long long now = time(0);
    ScriptVariable evname = ScriptVariable("last_") + ev;
    userinfo.SetItem(evname.c_str(), ScriptNumber(now));
    return SaveUserinfo();
}

static ScriptVariable serialize_script_map(const ScriptMap &map)
{
    ScriptVariable res = "";
    ScriptVariable key, val;
    ScriptMap::Iterator iter(map);
    while(iter.GetNext(key, val)) {
        if(val.IsInvalid())
            continue;
        ScriptVector lines(val, "\n", " \t\r");
        lines[0] = key + " = " + lines[0];
        res += lines.Join("\n+");
        res += "\n\n";
    }
    return res;
}

bool SessionData::Save(bool creation)
{
    if(creation)
        make_dir_if_necessary((dirname + "/" SESS_SUBDIR).c_str());
    ScriptVariable infostr = serialize_script_map(info);
    ScriptVariable fname = dirname + "/" SESS_SUBDIR "/" + sess_fn;
    int fd = open(fname.c_str(),
                  creation ? O_WRONLY|O_CREAT|O_EXCL : O_WRONLY|O_TRUNC,
                  0600);
    if(fd == -1)
        return false;
    int wc = write(fd, infostr.c_str(), infostr.Length());
    int cr = close(fd);
    return wc == infostr.Length() && cr != -1;
}

bool SessionData::SaveUserinfo()
{
    if(!HasUser() || !userinfo_read)
        return false;   // actually this is a bug!
    ScriptVariable userdataf =
        dirname + "/" USER_SUBDIR "/" + username + "/" USER_FILENAME;
    ScriptVariable infostr = serialize_script_map(userinfo);
    int fd = open(userdataf.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600);
    if(fd == -1)
        return false;
    int wc = write(fd, infostr.c_str(), infostr.Length());
    int cr = close(fd);
    return wc == infostr.Length() && cr != -1;
}

int SessionData::CheckAndReserveEmail(const ScriptVariable &email,
                                      const ScriptVariable &login)
{
    if(!email_data)
        email_data = new EmailData(dirname + "/" EMAIL_SUBDIR);
    EmailInfo emi;
    bool ok = email_data->Get(email, emi);
    if(!ok)
        return cu_conf_error;
    emi.status.Trim().Tolower();
    if(emi.status == "banned" || emi.status == "blocked")
        return cu_email_banned;
    if(emi.status == "used" || emi.status == "active")
        return cu_email_used;
    if((emi.status == "replaced" || emi.status == "pending_replaced")
        && emi.user != login)
    {
        return cu_email_replaced;
    }
    if(emi.status == "pending") {
        if(emi.date != -1 && time(0) - emi.date < tried_email_cooldown) {
            return cu_email_recently_tried;
        } else {
            // in this case, the address was claimed some time ago,
            // but then was never confirmed, so we allow for it
            // to be used (in case the new claimer will confirm the claim)

            // XXX XXX for pending, check THAT user, and update if necessary

            // that user may be active (with another address), in this
            // case there's nothing to be done

            // however, if it is still pending, AND the address matches,
            // we should perhaps just remove that user from the system
        }
    }
    if(emi.status.IsValid() && emi.status != "" &&
        emi.status != "pending" && emi.status != "replaced" &&
        emi.status != "pending_replaced")
    {
        return cu_conf_error;    // unknown email status
    }

    emi.user = login;
    if(emi.status == "replaced")
        emi.status = "pending_replaced";
    else
        emi.status = "pending";
    email_data->Set(email, emi);   // the date is updated there

    return cu_success;
}

int SessionData::CreateUser(ScriptVariable lgn, const ScriptVariable &name,
               const ScriptVariable &email, const ScriptVariable &site,
               ScriptVariable &confirmation_code)
{
    lgn.Tolower();
    if(logged_in || (HasUser() && username != lgn))
        return cu_bug;
    if(!check_username_acceptable(lgn.c_str()))
        return cu_bad_id;

    ConfigInformation prof;
    if(GetProfile(lgn, prof)) {  // well, something exists
        ScriptVariable status = prof.GetItem("status");
        if(status != "pending")
            return cu_exists;
        // for "pending", let's check the time
        long long t;
        bool ok = prof.GetItem("last_pwdsent").GetLongLong(t, 10);
        if(!ok || time(0) - t < pending_time_max)
            return cu_exists;
        // well, it is "pending" and too old, so just continue
    }

    int check_res = CheckAndReserveEmail(email, lgn);
    if(check_res != cu_success)
        return check_res;

    // it seems no more obstacles to go on

    username = lgn;
    logged_in = false;          // a bit of paranoia, it can't be true anyway
    EnsureUserinfoRead();       // just in case there's more info than we set
    userinfo_read = true;       // we force it for the case (almost always)
                                // there was nothing to read (no file)
    userinfo.SetItem("status", "pending");
    userinfo.SetItem("email", email);
    userinfo.SetItem("realname", name);
    userinfo.SetItem("created", ScriptNumber(time(0)));
    if(site.IsValid() && site != "")
        userinfo.SetItem("site", site);
    confirmation_code = make_random_password();
    userinfo.SetItem("confirmation_code", confirmation_code);

    make_dir_if_necessary((dirname + "/" USER_SUBDIR).c_str());
    make_dir_if_necessary((dirname + "/" USER_SUBDIR "/" + username).c_str());
    UpdateUserLastPwdsent();   // this will save userinfo

    info.SetItem("user", username);
    info.SetItem("logged_in", "no");
    Save(false);

    return cu_success;
}

bool SessionData::CanRequestEmailChange() const
{
    if(!logged_in)
        return false;

    EnsureUserinfoRead();
    ScriptVariable last_mc_str = userinfo.GetItem("last_mailchange");
    if(last_mc_str.IsValid() && last_mc_str != "") {
        long long last_mc;
        bool ok = last_mc_str.GetLongLong(last_mc, 10);
        if(ok && time(0) - last_mc < email_change_cooldown)
            return false;
    }

    return true;
}

int SessionData::EmailChangeRequest(const ScriptVariable &newemail,
                                    const ScriptVariable &passtoken,
                                    ScriptVariable &confirmation_code)
{
    if(!logged_in)
        return cu_bug;

    ScriptVariable pt = passtoken;
    pt.Toupper();
    if(!check_passtoken_safe(pt.c_str()))
        return cu_change_req_bad_passwd;

    bool passwd_ok = CheckAndRemovePassword(username, pt);
    if(!passwd_ok)
        return cu_change_req_bad_passwd;

    if(!CanRequestEmailChange())
        return cu_change_req_too_early;

    int check_res = CheckAndReserveEmail(newemail, username);
    if(check_res != cu_success)
        return check_res;

    confirmation_code = make_random_password();

    userinfo.SetItem("new_email", newemail);
    userinfo.SetItem("confirmation_code", confirmation_code);
    UpdateUserLastEvent("mailchange");   // this will save userinfo

    return cu_success;
}

bool SessionData::EmailChangeConfirm(ScriptVariable confirmation_code)
{
    confirmation_code.Trim().Toupper();
    EnsureUserinfoRead();
    ScriptVariable code = userinfo.GetItem("confirmation_code");
    code.Trim().Toupper();
    if(confirmation_code != code)
        return false;

    ScriptVariable old_email = userinfo.GetItem("email");
    ScriptVariable new_email = userinfo.GetItem("new_email");
    userinfo.SetItem("email", new_email);
    userinfo.SetItem("new_email", ScriptVariableInv());
    userinfo.SetItem("confirmation_code", ScriptVariableInv());
    SaveUserinfo();

    if(!email_data)
        email_data = new EmailData(dirname + "/" EMAIL_SUBDIR);
    EmailInfo emi;
    email_data->Get(old_email, emi);
    emi.status = "replaced";
    emi.user = username;
    email_data->Set(old_email, emi);
    email_data->Get(new_email, emi);
    emi.status = "active";
    emi.user = username;
    email_data->Set(new_email, emi);

    return true;
}

void SessionData::EmailChangeCancel()
{
    ScriptVariable new_email = userinfo.GetItem("new_email");
    userinfo.SetItem("new_email", ScriptVariableInv());
    userinfo.SetItem("confirmation_code", ScriptVariableInv());
    SaveUserinfo();

    if(!email_data)
        email_data = new EmailData(dirname + "/" EMAIL_SUBDIR);
    EmailInfo emi;
    email_data->Get(new_email, emi);
    if(emi.status == "pending_replaced") {
        emi.status = "replaced";
        email_data->Set(new_email, emi);
    } else {
        email_data->Forget(new_email);
    }
}

bool SessionData::UpdateUser(const ScriptVariable &name,
                             const ScriptVariable &site)
{
    if(!IsLoggedIn())
        return false;
    if(!EnsureUserinfoRead())
        return false;
    userinfo.SetItem("realname", name);
    if(site.IsValid() && site != "")
        userinfo.SetItem("site", site);
    UpdateUserLastSeen();
    return true;
}

bool
SessionData::GetProfile(ScriptVariable login, ConfigInformation &prof) const
{
    ScriptVariable fname =
        dirname + "/" USER_SUBDIR "/" + login + "/" USER_FILENAME;
    prof.Clear();
    if(!prof.FOpen(fname.c_str()))
        return false;
    bool rr = prof.RunParser();
    prof.FClose();
    return rr;
}


ScriptVariable SessionData::GetPremodQueueDir() const
{
    return dirname + "/" PREMOD_QUEUE_SUBDIR;
}

bool SessionData::CheckAndRemovePassword(const ScriptVariable &lgn,
                                         const ScriptVariable &pt)
{
    ScriptVariable passfname =
        dirname + "/" USER_SUBDIR "/" + lgn + "/" + pt;
    int res = unlink(passfname.c_str());
    return res != -1;
}

#if 0
static void place_to_premod(const PremodQueueData &qd, int id,
                            const ScriptVariable &cmt_fname)
{
    if(-1 == check_and_make_dir(qd.dir))
        return;
    ScriptVariable linkname =
        qd.dir + "/" + qd.realm + "=" + qd.page_id + "=" + ScriptNumber(id);
    ScriptVariable shortpath = short_link_path(cmt_fname, linkname);
    symlink(shortpath.c_str(), linkname.c_str());
}
#endif

void SessionData::AddToPremodQueue(const ScriptVariable &pgid, int cmtid,
                                   const ScriptVariable &cmt_file) const
{
    ScriptVariable dir = GetPremodQueueDir();
    if(!make_dir_if_necessary(dir.c_str()))
        return;
    ScriptVariable linkname =
        dir + "/" + pgid + "=" + ScriptNumber(cmtid);
    ScriptVariable shortpath = short_link_path(cmt_file, linkname);
    symlink(shortpath.c_str(), linkname.c_str());
}

int SessionData::GetPremodQueue(ScriptVector &res) const
{
    const char *s;
    ReadDir rd(GetPremodQueueDir().c_str());
    if(!rd.OpenOk())
        return -1;
    while((s = rd.Next())) {
        if(!*s || *s == '.' || *s == '_')
            continue;
        ScriptVariable ssv(s);
        res.AddItem(ssv);
    }
    return res.Length();
}

void SessionData::RemoveFromPremodQueue(const ScriptVariable &pgid, int cmtid)
{
    ScriptVariable dir = GetPremodQueueDir();
    if(!make_dir_if_necessary(dir.c_str()))
        return;
    ScriptVariable linkname =
        dir + "/" + pgid + "=" + ScriptNumber(cmtid);
    unlink(linkname.c_str());
}

bool SessionData::CheckAndStoreNonce(const ScriptVariable &timemark,
                                     const ScriptVariable &nonce)
{
    ScriptVariable dn(dirname + "/" NONCE_SUBDIR);
    make_dir_if_necessary(dn.c_str());
    ScriptVariable dummy_file(dn + "/_dummy");
    int fd = open(dummy_file.c_str(), O_WRONLY|O_CREAT, 0600);
    if(fd != -1)
        close(fd);
    ScriptVariable fname(dn + "/" + timemark + "_" + nonce);
    int r = link(dummy_file.c_str(), fname.c_str());
    if(r == -1)
        return false;

        ////////////////////////
        // -- cleanup --
        // note cleanup is only done after a new nonce stored successfully
    long long now = time(0);

    ReadDir rdir(dn.c_str());
    if(rdir.OpenOk()) {
        const char *s;
        while((s = rdir.Next())) {
            if(!*s || *s == '.' || *s == '_')
                continue;
            ScriptVector comp(s, "_");
            if(comp.Length() != 2)
                continue;
            long long t;
            if(!comp[0].GetLongLong(t, 10))
                continue;
            if(now - t > captcha_ttl + 1) {
                ScriptVariable fn(dn + "/" + s);
                unlink(fn.c_str());
            }
        }
    }
        // -- end of cleanup
        ////////////////////////

    return true;
}


////////////////////////////////////////////////////////
// cleanup implementation

#define CLEANUP_LOCK_FILENAME "__cleanup_lock"
#define CLEANUP_TIME_FILENAME "__next_cleanup"
#define CLEANUP_PERIOD (3*3600)
#define CLEANUP_LOCK_REMOVAL_ADDITION 600


static bool write_next_cleanup_time(const ScriptVariable &dir, long long tm)
{
    ScriptVariable fname = dir + "/" + CLEANUP_TIME_FILENAME;
    ScriptVariable fname_tmp = fname + "." + ScriptNumber(getpid());
    int fd = open(fname_tmp.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0600);
    if(fd == -1)
        return false;
    int wc = write(fd, &tm, sizeof(tm));
    close(fd);
    if(wc != sizeof(tm)) {
        unlink(fname_tmp.c_str());
        return false;
    }
    return -1 != rename(fname_tmp.c_str(), fname.c_str());
}

static long long get_next_cleanup_time(const ScriptVariable &dir)
{
    ScriptVariable fname = dir + "/" + CLEANUP_TIME_FILENAME;
    int fd = open(fname.c_str(), O_RDONLY);
    if(fd == -1)
        return -1;
    long long tm;
    int rc;
    rc = read(fd, &tm, sizeof(tm));
    close(fd);
    if(rc != sizeof(tm))
        return -1;
    return tm;
}

static void
remove_expired_session_files(const ScriptVariable &dir, long long now)
{
    const char *s;
    ReadDir rd(dir.c_str());
    while((s = rd.Next())) {
        ConfigInformation info;
        bool ok = info.FOpen((dir + "/" + s).c_str());
        if(!ok)
            continue;
        bool rd = info.RunParser();
        info.FClose();
        if(!rd)
            continue;
        ScriptVariable expire = info.GetItem("expire");
        bool remv;
        if(expire.IsInvalid())
            remv = true;
        else {
            long long et;
            if(expire.GetLongLong(et, 10))
                remv = et < now;
            else
                remv = true;
        }
        if(remv)
            unlink((dir + "/" + s).c_str());
    }
}

static bool have_group(unsigned int gid)
{
    if(getegid() == gid)
        return true;
    int cnt = getgroups(0, 0);
    gid_t *v = new gid_t[cnt];
    getgroups(cnt, v);
    int i;
    bool res = false;
    for(i = 0; i < cnt; i++) {
        if(gid == v[i]) {
            res = true;
            break;
        }
    }
    delete[] v;
    return res;
}

void SessionData::PerformCleanup(int ttl)
{
    if(dirname.IsInvalid() || dirname == "")
        return;

    struct stat st;
    int res;

    long long now = time(0);

    ScriptVariable sessdirname = dirname + "/" SESS_SUBDIR;

        // most of the time we don't proceed because the time didn't come
        // so this check is to be done first
    long long tt = get_next_cleanup_time(sessdirname);
    if(tt != -1 && tt >= now)
        return;

    // now check if the directory exists and is accessible
    // note that we really need full access to the directory, including
    // read access, or else we will not be able to perform cleanup
    res = stat(sessdirname.c_str(), &st);
    if(res == -1)
        return;
    bool accessible =                ((0007 & st.st_mode) == 0007)  ||
        (have_group(st.st_gid)    && ((0070 & st.st_mode) == 0070)) ||
        ((geteuid() == st.st_uid) && ((0700 & st.st_mode) == 0700));
    if(!accessible)
        return;

    // now check the lock (may be someone else is already doing this)
    ScriptVariable lockfname = sessdirname + "/" CLEANUP_LOCK_FILENAME;
    res = stat(lockfname.c_str(), &st);
    if(res != -1) {
        // Lock exists, but if it is too old, we will remove it
        // so that someone else will be able to perform the cleanup when
        // the time comes.  In this case, we'll add some time to the
        // next cleanup time so that some time has to pass before
        // someone actually clenup
        if(st.st_mtime < now - CLEANUP_PERIOD) {
            long long tt = now + CLEANUP_LOCK_REMOVAL_ADDITION;
            if(!write_next_cleanup_time(sessdirname, tt))
                return;
            sync();
            unlink(lockfname.c_str());
        }
        return;
    }

    // well, seems we're to go!  try to get the lock

    int fd = open(lockfname.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0600);
    if(fd == -1)    // failed to get the lock, let someone else try :-)
        return;
    close(fd);

    if(!write_next_cleanup_time(sessdirname, now + CLEANUP_PERIOD))
        return;

    // now the cleanup!!!
    remove_expired_session_files(sessdirname, now);

    // remove the lock
    unlink(lockfname.c_str());
}

