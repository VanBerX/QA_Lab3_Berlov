#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrmacro.hpp>
#include <scriptpp/scrmsg.hpp>
#include <scriptpp/cmd.hpp>

#include "xcgi.hpp"
#include "xcaptcha.hpp"
#include "xrandom.h"
#include "tcgi_db.hpp"
#include "tcgi_ses.hpp"
#include "tcgi_rpl.hpp"
#include "invoke.h"
#include "emailval.h"
#include "fnchecks.h"

#ifndef THALASSA_CGI_CONFIG_PATH
#define THALASSA_CGI_CONFIG_PATH "thalcgi.ini"
#endif

#ifndef THALASSA_CGI_SESSION_TTL
#define THALASSA_CGI_SESSION_TTL (2*24*3600)
#endif

#ifndef THALASSA_CGI_SESSID_COOKIE
#define THALASSA_CGI_SESSID_COOKIE "thalassa_sessid"
#endif

#ifndef THALASSA_PASSWD_RESEND_MIN
#define THALASSA_PASSWD_RESEND_MIN (24*3600)
#endif

#ifndef THALASSA_PASSWD_GEN_COUNT
#define THALASSA_PASSWD_GEN_COUNT 20
#endif


/////////////////////////////////////////////////////////////////
// functions that actually send the response
//
// NOTE there are exactly 5 possibilities to finish the job:
//   send_error_page, send_nocookie_page, send_retrycaptcha_page,
//   send_the_page and send_result_page
//

    // NB: in case of error, no cookie, or failed captcha,
    // we do nothing with cookies and hence we don't need the session

static void send_error_page(Cgi &cgi, const ThalassaCgiDb &db,
                            int code, const char *cmt)
{
    ScriptVariable page = db.MakeErrorPage(code, cmt);
    cgi.SetStatus(code, cmt);
    cgi.SetBody(page);
    cgi.Commit();
}

static void send_nocookie_page(Cgi &cgi, const ThalassaCgiDb &db)
{
    ScriptVariable page = db.MakeNocookiePage();
    cgi.SetBody(page);
    cgi.Commit();
}

static void send_retrycaptcha_page(Cgi &cgi, const ThalassaCgiDb &db,
                                   int captcha_res)
{
    ScriptVariable page = db.MakeRetryCaptchaPage(captcha_res);
    cgi.SetBody(page);
    cgi.Commit();
}


    // every non-"error_page" response is done through this function
static void commit_response(Cgi &cgi, SessionData &session,
                            const ScriptVariable &pg)
{
    if(session.IsValid()) {
        cgi.SetCookie(THALASSA_CGI_SESSID_COOKIE, session.GetId().c_str(),
                      THALASSA_CGI_SESSION_TTL, true, false);
    } else {
        if(session.JustRemoved())
            cgi.DiscardCookie(THALASSA_CGI_SESSID_COOKIE);
    }
    cgi.SetBody(pg);
    cgi.Commit();
}

static void renew_if_needed(SessionData &session)
{
    if(session.IsValid() && !session.JustCreated())
        session.Renew(THALASSA_CGI_SESSION_TTL);
}

static void send_the_page(Cgi &cgi, const ThalassaCgiDb &db,
                          PathData &page, SessionData &session)
{
    renew_if_needed(session);
    ScriptVariable pg = db.BuildPage(page);
    commit_response(cgi, session, pg);
}

static void send_result_page(Cgi &cgi, const ThalassaCgiDb &db,
                             PathData &page, SessionData &session,
                             const char *msgid, bool is_it_ok)
{
    renew_if_needed(session);
    ScriptVariable pg = db.BuildResultPage(page, msgid, is_it_ok);
    commit_response(cgi, session, pg);
}


// end of the response-sending infrastructure
/////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////
// POST request processing implementation -- yes, it is huge
//

static void process_set_cookie_request(Cgi &cgi, const ThalassaCgiDb &db,
                                       SessionData &sess, PathData &page)
{

    ScriptVariable form_ip    = cgi.GetParam("captcha_ip");
    ScriptVariable form_time  = cgi.GetParam("captcha_time");
    ScriptVariable form_nonce = cgi.GetParam("captcha_nonce");
    ScriptVariable form_token = cgi.GetParam("captcha_token");
    ScriptVariable response   = cgi.GetParam("captcha_response");

    int cap_res =
        captcha_validate(form_ip, form_time, form_nonce, form_token, response);
    if(cap_res != captcha_result_ok) {
        send_retrycaptcha_page(cgi, db, cap_res);
        return;
    }

    bool nonce_ok = sess.CheckAndStoreNonce(form_time, form_nonce);
    if(!nonce_ok) {
        send_error_page(cgi, db, 500, "Nonce check error");
        return;
    }


    bool ok = sess.Create(THALASSA_CGI_SESSION_TTL);
    if(ok) {
            // It is definitely unneeded to perform MORE cleanup scans
            // than we set cookies.
            // So, this is THE place we do it
        sess.PerformCleanup(THALASSA_CGI_SESSION_TTL);

            // send that 'cookie set' message to the user
        send_result_page(cgi, db, page, sess, "cookie_set", true);
    } else {
        send_error_page(cgi, db, 500, "Session creation error");
    }
}


static bool
get_and_check_field(Cgi &cgi, const ThalassaCgiDb &db,
                    PathData &page, SessionData &sess,
                    const char *name, ScriptVariable &field)
{
    field = cgi.GetParam(name);
    if(field.IsInvalid() || field.Trim() == "") {
        send_result_page(cgi, db, page, sess, "field_not_filled", false);
        return false;
    }
    return true;
}

static bool
get_and_check_feedback_category(Cgi &cgi, const ThalassaCgiDb &db,
                                PathData &page, SessionData &sess,
                                ScriptVariable &receiver_addr)
{
    ScriptVariable cat = cgi.GetParam("category");
    if(cat.IsValid() && cat.Trim() != "") {
        receiver_addr = db.GetContactRecipient(cat);
        receiver_addr.Trim();
    } else {
        receiver_addr.Invalidate();
    }
    if(receiver_addr.IsInvalid() || receiver_addr == "") {
        send_result_page(cgi, db, page, sess,
                         "invalid_feedback_category", false);
        return false;
    }
    return true;
}

enum { email_text_width = 75 };

static ScriptVariable join_to_columns(const ScriptVector &v)
{
    static const char colsep[] = "   ";
    ScriptVariable res;
    ScriptVariable s;
    int vlen = v.Length();
    int i;
    for(i = 0; i < vlen; i++) {
        if(s == "") {
            s = v[i];
            continue;
        }
        int lnlen = s.Length() + (sizeof(colsep)-1) + v[i].Length();
        if(lnlen <= email_text_width) {
            s += colsep;
            s += v[i];
        } else {
            s += "\n";
            res += s;
            s = v[i];
        }
    }
    if(s != "") {
        s += "\n";
        res += s;
    }
    return res;
}

static bool send_service_email(const ThalassaCgiDb &db, const char *id,
                      const ScriptVariable &email, const ScriptVector &dict)
{
    ScriptVector sendmail_cmd;
    ScriptVariable body;
    db.BuildServiceEmail(id, email, dict, sendmail_cmd, body);
    char **argv = sendmail_cmd.MakeArgv();
    int res = invoke_command(argv, body.c_str(), body.Length());
    ScriptVector::DeleteArgv(argv);
    return res == 0;
}

static void process_send_passwords(Cgi &cgi, const ThalassaCgiDb &db,
                                   SessionData &sess, PathData &page)
{
    ScriptVariable login;
    if(!get_and_check_field(cgi, db, page, sess, "login", login))
        return;
    bool ok;
    ok = sess.SetUser(login);
    if(!ok) {
        send_result_page(cgi, db, page, sess, "login_unknown", false);
        return;
    }
    int rem_passwds = sess.GetUserRemainingPasswords();
    if(rem_passwds < 0) {
        send_result_page(cgi, db, page, sess, "server_side_error", false);
        return;
    }
    long long last_sent = sess.GetUserLastPwdsent();
    long long now = time(0);
    if(rem_passwds > 0 && last_sent > 0 &&
        now - last_sent < THALASSA_PASSWD_RESEND_MIN)
    {
        send_result_page(cgi, db, page, sess, "pass_req_too_early", false);
        return;
    }

    ScriptVector passw;
    ok = sess.GenerateNewPasswords(passw, THALASSA_PASSWD_GEN_COUNT);
    if(!ok) {
        send_result_page(cgi, db, page, sess, "server_side_error", false);
        return;
    }

    ScriptVariable user_email = sess.GetUserEmail();
    if(user_email.IsInvalid() || user_email == "") {
        send_result_page(cgi, db, page, sess, "server_side_error", false);
        return;
    }

    ScriptVector dict;
    dict.AddItem("passwords");
    dict.AddItem(join_to_columns(passw));
    dict.AddItem("event");
    dict.AddItem("passwords");

    ok = send_service_email(db, "passwords", user_email, dict);

    if(ok) {
        sess.UpdateUserLastPwdsent();
        send_result_page(cgi, db, page, sess, "email_sent_to_you", true);
    } else {
        send_result_page(cgi, db, page, sess, "error_sending_email", false);
    }
}

static void process_login(Cgi &cgi, const ThalassaCgiDb &db,
                          SessionData &sess, PathData &page)
{
    ScriptVariable login, passtoken;
    if(!get_and_check_field(cgi, db, page, sess, "login", login))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "passtoken", passtoken))
        return;
    bool ok = sess.Login(login, passtoken);
    send_result_page(cgi, db, page, sess,
                     ok ? "logged_in" : "wrong_login_password", ok);
}

static const char *diag_id_by_user_creation_result(int ucr)
{
    switch(ucr) {
    case SessionData::cu_bad_id:                return "invalid_login_name";
    case SessionData::cu_exists:                return "login_not_available";
    case SessionData::cu_bad_email:             return "invalid_email_address";
    case SessionData::cu_email_used:            return "email_already_used";
    case SessionData::cu_email_replaced:      return "email_belonged_to_other";
    case SessionData::cu_email_recently_tried:  return "email_recently_tried";
    case SessionData::cu_email_banned:          return "email_banned";
    case SessionData::cu_change_req_bad_passwd: return "wrong_password";
    case SessionData::cu_change_req_too_early:  return "changemail_too_early";
    case SessionData::cu_conf_error:            return "server_side_error";
    case SessionData::cu_bug:
    default:                                    return "bug_in_the_code";
    }
}

static void process_signup(Cgi &cgi, const ThalassaCgiDb &db,
                           SessionData &sess, PathData &page)
{
    ScriptVariable userid, username, useremail, usersite;
    if(!get_and_check_field(cgi, db, page, sess, "userid", userid))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "username", username))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "useremail", useremail))
        return;
    usersite = cgi.GetParam("usersite");
    if(usersite.IsInvalid())
        usersite = "";
    int errcode, errpos; /* will be ignored */
    if(!email_address_validate(useremail.c_str(), &errcode, &errpos)) {
        send_result_page(cgi, db, page, sess, "invalid_email_address", false);
        return;
    }

    ScriptVariable code;
    int cr = sess.CreateUser(userid, username, useremail, usersite, code);
    if(cr != SessionData::cu_success) {
        const char *diag_id = diag_id_by_user_creation_result(cr);
        send_result_page(cgi, db, page, sess, diag_id, false);
        return;
    }

    ScriptVector dict;
    dict.AddItem("confirmcode");
    dict.AddItem(code);
    dict.AddItem("event");
    dict.AddItem("signup");

    bool ok = send_service_email(db, "confirm", useremail, dict);
    send_result_page(cgi, db, page, sess,
        ok ? "email_sent_to_you" : "error_sending_email", ok);
}

static void process_save_profile(Cgi &cgi, const ThalassaCgiDb &db,
                                 SessionData &sess, PathData &page)
{
    if(!sess.IsLoggedIn()) {
        send_result_page(cgi, db, page, sess, "not_logged_in", false);
        return;
    }
    ScriptVariable username, usersite;
    if(!get_and_check_field(cgi, db, page, sess, "username", username))
        return;
    usersite = cgi.GetParam("usersite");
    bool ok = sess.UpdateUser(username, usersite);
    send_result_page(cgi, db, page, sess,
                     ok ? "new_values_saved" : "server_side_error", ok);
}

static void process_change_mail(Cgi &cgi, const ThalassaCgiDb &db,
                                SessionData &sess, PathData &page)
{
    if(!sess.IsLoggedIn()) {
        send_result_page(cgi, db, page, sess, "not_logged_in", false);
        return;
    }
    ScriptVariable newemail, passtoken;
    if(!get_and_check_field(cgi, db, page, sess, "newemail", newemail))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "passtoken", passtoken))
        return;

    int errcode, errpos; /* will be ignored */
    if(!email_address_validate(newemail.c_str(), &errcode, &errpos)) {
        send_result_page(cgi, db, page, sess, "invalid_email_address", false);
        return;
    }

    ScriptVariable code;
    int cr = sess.EmailChangeRequest(newemail, passtoken, code);

    if(cr != SessionData::cu_success) {
        const char *diag_id = diag_id_by_user_creation_result(cr);
        send_result_page(cgi, db, page, sess, diag_id, false);
        return;
    }

    ScriptVector dict;
    dict.AddItem("confirmcode");
    dict.AddItem(code);
    dict.AddItem("event");
    dict.AddItem("changemail");

    bool ok = send_service_email(db, "confirm", newemail, dict);
    send_result_page(cgi, db, page, sess,
                     ok ? "email_sent_to_you" : "error_sending_email", ok);
}


static bool check_really_confirmed(Cgi &cgi, const ThalassaCgiDb &db,
                                   SessionData &sess, PathData &page)
{
    ScriptVariable really;
    if(!get_and_check_field(cgi, db, page, sess, "really", really))
        return false;
    really.Trim().Tolower();
    if(really != "really") {
        send_result_page(cgi, db, page, sess, "not_really_confirm", false);
        return false;
    }
    return true;
}


static void process_confirm_mail_change(Cgi &cgi, const ThalassaCgiDb &db,
                                        SessionData &sess, PathData &page)
{
    if(!sess.IsLoggedIn()) {
        send_result_page(cgi, db, page, sess, "not_logged_in", false);
        return;
    }
    if(!sess.IsChangingEmail()) {
        send_result_page(cgi, db, page, sess, "not_this_way", false);
        return;
    }
    if(cgi.GetParam("cancel_change") == "yes") {
        bool really = check_really_confirmed(cgi, db, sess, page);
        if(!really)
            return;
        sess.EmailChangeCancel();
        send_result_page(cgi, db, page, sess, "changemail_canceled", true);
        return;
    }
    ScriptVariable confirmcode;
    if(!get_and_check_field(cgi, db, page, sess, "confirmcode", confirmcode))
        return;
    bool ok = sess.EmailChangeConfirm(confirmcode);
    send_result_page(cgi, db, page, sess,
                     ok ? "email_changed" : "wrong_confirmation_code", ok);
}

static void process_send_email(Cgi &cgi, const ThalassaCgiDb &db,
                               SessionData &sess, PathData &page)
{
    ScriptVariable name, email, subject, receiver_addr, body;
        // nb: email here means the sender's address typed into the form

    if(!get_and_check_field(cgi, db, page, sess, "name", name))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "mail", email))
        return;
    int errcode, errpos; /* will be ignored */
    if(!email_address_validate(email.c_str(), &errcode, &errpos)) {
        send_result_page(cgi, db, page, sess, "invalid_email_address", false);
        return;
    }
    if(!get_and_check_field(cgi, db, page, sess, "subject", subject))
        return;
    if(!get_and_check_feedback_category(cgi, db, page, sess, receiver_addr))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "msgbody", body))
        return;

    ScriptVector sendmail_cmd;
    ScriptVariable cmdin;
    db.BuildContactFormMessage(receiver_addr, sendmail_cmd, cmdin);
    char **argv = sendmail_cmd.MakeArgv();
    bool ok = 0 == invoke_command(argv, cmdin.c_str(), cmdin.Length());
    ScriptVector::DeleteArgv(argv);

    send_result_page(cgi, db, page, sess,
                     ok ? "your_email_sent" : "error_sending_email", ok);
}

static bool run_regeneration(const ThalassaCgiDb &db)
{
    ScriptVector command;
    bool ok = db.MakeCommentPageRegenCmd(command);
    if(!ok)
        return false;
    char **argv = command.MakeArgv();
    int res = invoke_command(argv, "", 0);
    ScriptVector::DeleteArgv(argv);
    return res == 0;
}

static void process_comment_add(Cgi &cgi, ThalassaCgiDb &db,
                                SessionData &sess, PathData &page,
                                const ScriptVector &action)
{
    // NB: action[1] == cmt_id
    if(action[1].IsValid() && action[1] != "" &&
        !check_fname_safe(action[1].c_str()))
    {
        send_error_page(cgi, db, 406, "path not acceptable");
        return;
    }

    bool ok;
    DiscussionInfo reply_info;
    ok = db.GetDiscussionInfo(action[1], reply_info);
    if(!ok) {
        send_result_page(cgi, db, page, sess, "not_this_way", false);
        return;
    }
    bool bypass_premod;
    ok = db.CanPost(bypass_premod);
    if(!ok) {
        send_result_page(cgi, db, page, sess,
            sess.IsLoggedIn() ? "permission_denied" : "anon_denied", false);
        return;
    }

    ScriptVariable subject, body;
    if(!get_and_check_field(cgi, db, page, sess, "subject", subject))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "cmtbody", body))
        return;

    NewCommentData com_data;

    if(sess.IsLoggedIn()) {
        com_data.user_id = sess.GetUser();
        com_data.user_name = sess.GetUserRealname();
    } else {
        com_data.user_id.Invalidate();
        ScriptVariable name;
        if(!get_and_check_field(cgi, db, page, sess, "name", name))
            return;
        com_data.user_name = name;
        sess.SaveUsedRealname(name);
    }
    com_data.title = subject;
    com_data.body = body;

    com_data.parent_comment_id = 0;
    if(action[1].IsValid() && action[1] != "") {
        long par;
        if(action[1].GetLong(par, 10))
            com_data.parent_comment_id = par;
    }

    com_data.creator_addr =
        cgi.GetRemoteAddr() + ":" + ScriptNumber(cgi.GetRemotePort());
    com_data.creator_session = sess.GetId0();
    com_data.creator_date = ScriptNumber(time(0));

    if(cgi.GetParam("preview") == "yes") {
        db.SetPreview(com_data);
        send_the_page(cgi, db, page, sess);
        return;
    }

    if(bypass_premod) {
        int res = save_new_comment(reply_info.cmt_tree_dir, com_data, 0);
           // NB: here we intentionally ignore possible regeneration errors
        run_regeneration(db);
        if(res > 0)
            db.SetJustPostedSubst(ScriptNumber(res), false);
        send_result_page(cgi, db, page, sess,
            res > 0 ? "comment_saved" : "server_side_error", res > 0);
    } else {
        ScriptVariable cmtf;
        int res = save_new_comment(reply_info.cmt_tree_dir, com_data, &cmtf);
        if(res > 0) {
            sess.AddToPremodQueue(reply_info.premodq_page_id, res, cmtf);
            ScriptNumber cmt_id(res);
            db.SetJustPostedSubst(cmt_id, true);
        }

        send_result_page(cgi, db, page, sess,
            res > 0 ? "comment_queued_for_premod" : "server_side_error",
            res > 0);
    }
}

/* may be apply_moderation_request should be moved to the tcgi_rpl module */

static void apply_moderation_request(ScriptVariable moder,
                                     HeadedTextMessage &comment,
                                     bool &should_dequeue)
{
    ScriptVariable flagsstr = comment.FindHeader("flags");
    if(flagsstr.IsInvalid())
        flagsstr = "";
    ScriptVector flags(flagsstr, ",", " \t\r\n");
    ScriptVector newflags;
    int fl = flags.Length();
    bool hidden = false;
    bool premod = false;
    int i;
    for(i = 0; i < fl; i++) {
        if(flags[i] == "hidden") {
            hidden = true;
            continue;
        }
        if(flags[i] == "premod") {
            premod = true;
            continue;
        }
        if(flags[i] == "")
            continue;
            // preserve out-of-interest flags, if any
        newflags.AddItem(flags[i]);
    }

    should_dequeue = false;
    moder.Tolower();
    moder.Trim();
    if(moder == "hide") {
        newflags.AddItem("hidden");
        if(premod)
            newflags.AddItem("premod");
    } else
    if(moder == "unhide") {
        if(premod)
            should_dequeue = true;
    } else
    if(moder == "dequeue") {
        newflags.AddItem("hidden");
        should_dequeue = true;
    } else {
        // request unknown... it's an error, but we just leave all as is
        if(hidden)
            newflags.AddItem("hidden");
        if(premod)
            newflags.AddItem("premod");
    }
    comment.SetHeader("flags", newflags.Join(", "));
}

static void process_comment_edit(Cgi &cgi, const ThalassaCgiDb &db,
                                 SessionData &sess, PathData &page,
                                 const ScriptVector &action)
{
    // NB: action[1] == cmt_id
    if(!check_fname_safe(action[1].c_str()))
    {
        send_error_page(cgi, db, 406, "path not acceptable");
        return;
    }

    bool ok;
    DiscussionInfo discuss_info;
    ok = db.GetDiscussionInfo(action[1], discuss_info);
    if(!ok || discuss_info.comment_id < 1) {
        send_result_page(cgi, db, page, sess, "not_this_way", false);
        return;
    }

    HeadedTextMessage comment_file;
    ok = get_comment(discuss_info, comment_file);
    if(!ok) {
        send_result_page(cgi, db, page, sess, "server_side_error", false);
        return;
    }

    ScriptVariable moder = cgi.GetParam("moderation");
    if(moder.IsValid()) {   // this is a moderation request, that's simple
        if(!db.CanModerate()) {
            send_error_page(cgi, db, 403, "Forbidden");
            return;
        }
        if(moder == "regenerate") {
            ok = run_regeneration(db);
            send_result_page(cgi, db, page, sess,
                ok ? "page_regenerated" : "server_side_error", ok);
            return;
        }
        bool should_dequeue = false;
        apply_moderation_request(moder, comment_file, should_dequeue);
        ok = save_comment(discuss_info, comment_file);
        if(!ok) {
            send_result_page(cgi, db, page, sess, "server_side_error", false);
            return;
        }
        run_regeneration(db);  // result ignored intentionally!
        db.CurrentCommentChanged(action[1]);
        if(should_dequeue)
            sess.RemoveFromPremodQueue(discuss_info.premodq_page_id,
                                       discuss_info.comment_id);
        send_result_page(cgi, db, page, sess, "new_values_saved", true);
        return;
    }
    // okay, now it is either a save or a delete request
    ScriptVariable cmt_owner;
    long long cmt_unixdate;
    get_owner_and_date_from_htm(comment_file, cmt_owner, cmt_unixdate);

    if(!db.CanEdit(cmt_owner, cmt_unixdate)) {
        send_error_page(cgi, db, 403, "Forbidden");
        return;
    }

    // check if deletion is requested
    ScriptVariable delreq = cgi.GetParam("delete");
    if(delreq.IsValid() && delreq != "") {
        delreq.Trim().Tolower();
        if(delreq != "yes") {
            send_result_page(cgi, db, page, sess, "not_this_way", false);
            return;
        }

        bool really = check_really_confirmed(cgi, db, sess, page);
        if(!really) // the result is sent by check_really_confirmed
            return;

            // please note this is the only correct sequence:
            // FIRST we delete the comment, then (when the queue isn't
            // yet modified) we freeze the moder_queue position, and
            // only then we actually remove the item from the moder_queue
        ok = delete_comment(discuss_info);
        db.CurrentCommentChanged(action[1]);
        sess.RemoveFromPremodQueue(discuss_info.premodq_page_id,
                                   discuss_info.comment_id);
            // XXX actually, we should check if it was visible!
            //     no regeneration is needed after deleting hidden comment
        run_regeneration(db);  // result ignored intentionally
        send_result_page(cgi, db, page, sess,
                         ok ? "comment_deleted" : "server_side_error", ok);
        return;
    }

    // the last possibility is content changing request
    ScriptVariable subject, body;
    if(!get_and_check_field(cgi, db, page, sess, "subject", subject))
        return;
    if(!get_and_check_field(cgi, db, page, sess, "cmtbody", body))
        return;

    ScriptVariable logmark =
        cgi.GetRemoteAddr() + ":" + ScriptNumber(cgi.GetRemotePort()) +
        " " + ScriptNumber(time(0)) + " " + sess.GetId0() + " " +
        (sess.IsLoggedIn() ? sess.GetUser() : "-");
    replace_comment_content(comment_file, subject, body, logmark);

    ok = save_comment(discuss_info, comment_file);
    db.CurrentCommentChanged(action[1]);
       // NB: here we intentionally ignore possible regeneration errors
    run_regeneration(db);
    send_result_page(cgi, db, page, sess,
                     ok ? "new_values_saved" : "server_side_error", ok);
}

#if 0
static void process_whatever_unimplemented(Cgi &cgi, const ThalassaCgiDb &db,
                                           SessionData &sess, PathData &page)
{
    send_result_page(cgi, db, page, sess, "not_implemented_yet", false);
}
#endif

static void process_post_request(Cgi &cgi, ThalassaCgiDb &db,
                                 SessionData &sess, PathData &page)
{
    if(!sess.IsValid()) {
        int lim = db.GeneralPostLimit();
        if(cgi.ContentLength() > lim * 1024) {
            send_error_page(cgi, db, 413, "request entity too large");
            return;
        }
        cgi.ParseBody();
            // setcookie is the only case when POST request is
            // processed despite there's no valid session
        if(cgi.GetParam("command") == "setcookie") {
            process_set_cookie_request(cgi, db, sess, page);
            return;
        }
            // no other POST requests are allowed to proceed
            // outside of a session, so just refuse it
        send_nocookie_page(cgi, db);
        return;
    }

    if(!page.post_allowed) {
        send_error_page(cgi, db, 405, "method not allowed");
        return;
    }

    ScriptVector action;
    if(!db.GetPageAction(page, action)) {
        send_error_page(cgi, db, 500, "No action defined for this path");
        return;
    }

    fprintf(stderr, "ACTION: ");
    int i;
    for(i = 0; i < action.Length(); i++)
        fprintf(stderr, "[%s]", action[i].c_str());
    fprintf(stderr, "\n");

    // requests that don't need the body go first
    //     as of now, only rmsession falls to this category

    if(action[0] == "rmsession") {
        sess.Remove();
        send_result_page(cgi, db, page, sess, "cookie_removed", true);
        return;
    }

    // now's the time to check for the content length limit, because
    // all other requests use request bodies

    if(page.post_content_limit >= 0 &&
        cgi.ContentLength() > page.post_content_limit * 1024)
    {
        send_error_page(cgi, db, 413, "request entity too large");
        return;
    }

    // XXXXX perhaps we need to check here for requests that involve
    //       the multipart/form-data format (e.g. for file upload),
    //       as they need to setup file download callbacks and establish
    //       per-field limits before calling ParseBody


    // the rest of the requests don't need any special care, and
    // the content length is checked already, so feel free to parse the body

    cgi.ParseBody();

    if(action[0] == "login") {
        if(cgi.GetParam("sendmorepass") == "yes") {
            process_send_passwords(cgi, db, sess, page);
            return;
        }
        process_login(cgi, db, sess, page);
        return;
    }
    if(action[0] == "signup") {
        process_signup(cgi, db, sess, page);
        return;
    }
    if(action[0] == "profile") {
        if(action.Length() > 1 && action[1].IsValid() && action[1] != "") {
            send_result_page(cgi, db, page, sess, "not_this_way", true);
            return;
        }
        process_save_profile(cgi, db, sess, page);
        return;
    }
    if(action[0] == "changemail") {
        if(sess.IsChangingEmail())
            process_confirm_mail_change(cgi, db, sess, page);
        else
            process_change_mail(cgi, db, sess, page);
        return;
    }
    if(action[0] == "feedback") {
        if(cgi.GetParam("catchange") == "yes") {
            send_the_page(cgi, db, page, sess);
            return;
        }
        process_send_email(cgi, db, sess, page);
        return;
    }
    if(action[0] == "comment_add") {
        process_comment_add(cgi, db, sess, page, action);
        return;
    }
    if(action[0] == "comment_edit") {
        process_comment_edit(cgi, db, sess, page, action);
        return;
    }
    send_error_page(cgi, db, 500, "unknown action, check the cgi config");
}

//
// end of the POST request processing implementation
/////////////////////////////////////////////////////////////////



/////////////////////////////////////////////////////////////////
// subroutines for the very start
//

static void try_session_cookie(Cgi &cgi, SessionData &session)
{
    ScriptVariable c = cgi.GetCookie(THALASSA_CGI_SESSID_COOKIE);
    if(c.IsValid() && c != "")
        session.Validate(c);

    // don't forget: renewing the session right here is not
    // a good idea because it is possible we run into error later
}

  // OK, processing the GET is much much easier than for POST
  //
static void process_get_request(Cgi &cgi, const ThalassaCgiDb &db,
                                SessionData &sess, PathData &page)
{
    if(page.session_required && !sess.IsValid()) {
        send_nocookie_page(cgi, db);
        return;
    }

    send_the_page(cgi, db, page, sess);
}


/////////////////////////////////////////////////////////////////
// the main function infrastructure


static void captcha_setup(ThalassaCgiDb &db, SessionData &sess)
{
    ScriptVariable secret;
    int time_to_live;
    db.GetCaptchaParameters(secret, time_to_live);
    set_captcha_info(secret, time_to_live);
    sess.SetCaptchaTtl(time_to_live);
}

static bool check_conffile_mode()
{
    FileStat st(THALASSA_CGI_CONFIG_PATH);
    if(!st.Exists())
        return true;  /* it doesn't exist, yet it's NOT world-accessible */
    int uid, gid, mode;
    st.GetCreds(uid, gid, mode);
    int my_uid = getuid();
    if(my_uid != uid)                  /* perhaps there's no suexec here */
        return true;    /* this doesn't mean all right, just check abort */
    return (mode & 0007) == 0;
}

int main()
{
    Cgi cgi;
    ThalassaCgiDb db(&cgi);

    if(!check_conffile_mode()) {
        send_error_page(cgi, db, 500,
                        "Check permissions of your config file "
                            THALASSA_CGI_CONFIG_PATH);
        return 0;
    }

    if(!db.Load(THALASSA_CGI_CONFIG_PATH)) {
        ScriptVariable diag = db.MakeErrorMessage();
        send_error_page(cgi, db, 500, diag.c_str());
        return 0;
    }

    randomize();

    if(!cgi.ParseHead()) {
        cgi.Commit();
        return 0;
    }
#if 0
    db.SetRequest(&cgi);
#endif

    ScriptVariable datadir = db.GetUserdataDirectory();
    FileStat dds(datadir.c_str());
    if(!dds.Exists() || !dds.IsDir()) {
        send_error_page(cgi, db, 500, "Check userdata directory");
        return 0;
    }

    ScriptVariable sessdir = db.GetUserdataDirectory();
    FileStat sds(sessdir.c_str());
    if(!sds.Exists()) {
        int r = mkdir(sessdir.c_str(), 0700);
        if(r == -1) {
            send_error_page(cgi, db, 500, "Can't make sessions directory");
            return 0;
        }
    } else
    if(!sds.IsDir()) {
        send_error_page(cgi, db, 500, "Sessions dir isn't a dir O_o");
        return 0;
    }
    SessionData session(sessdir.c_str());
    db.SetSession(&session);

    captcha_setup(db, session); // XXX the second arg. to be removed one day



    try_session_cookie(cgi, session);

    fprintf(stderr, "PATH: [%s]\n", cgi.GetPath().c_str());

    PathData page;
    int pathres = db.FindPath(cgi.GetPath(), page);
    switch(pathres) {
    case ThalassaCgiDb::path_ok:
        break;
    case ThalassaCgiDb::path_bad:
        send_error_page(cgi, db, 400, "requested path bad");
        return 0;
    case ThalassaCgiDb::path_noent:
        send_error_page(cgi, db, 404, "path not configured");
        return 0;
    case ThalassaCgiDb::path_noaccess:
        send_error_page(cgi, db, 403, "forbidden");
        return 0;
    case ThalassaCgiDb::path_notfound:
        send_error_page(cgi, db, 404, "path not found");
        return 0;
    case ThalassaCgiDb::path_invalid:
        send_error_page(cgi, db, 406, "path not acceptable");
        return 0;
    default:
        send_error_page(cgi, db, 500, "bug in the CGI code");
        return 0;
    }

    if(cgi.IsPost())
        process_post_request(cgi, db, session, page);
    else
        process_get_request(cgi, db, session, page);

    return 0;
}
