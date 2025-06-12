#ifndef TCGI_RPL_HPP_SENTRY
#define TCGI_RPL_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

/*
    DiscussionInfo represents "something that can be replied to",
    either a comment page where the user wishes to leave a top-level
    comment, or an existing comment, to which the user wishes to
    reply.

    Technically DiscussionInfo data is derived from ``page id''
    (such as node id), the comment id, and all the data taken from
    [comments] section of the configuration file;  certainly,
    all macro variables are expanded before the configuration info
    goes to these fields.

    NOTE in case comment_id != 0, page_html_file and
    both html_*_mark fields are not used, and page_source is
    only used to determine whether comments are enabled (from the header);
    the 'original' text is taken from the comment file;

    page_source may be invalid, then it is assumed the comments are
    enabled; if comment_id == 0, then page_html_file and the marks
    will be used, so they must be filled in;

    in case page_source is valid, page_html_file _may_ be invalid,
    so the 'original' text will be extracted from page_source; but
    if it is valid, then it will be used, so marks must be valid, too.

    The workflow is as follows:
      0) make sure the ids you've got are filename-safe;
      1) call the database to get your DiscussionInfo;
    then EITHER
      2a) call get_discuss_display_data to get DiscussDisplayData;
      3a) display the comment // accept a new comment for storing
    OR
      2b) call get_comment
      3b) make changes and call save_comment
 */
struct DiscussionInfo {
    int comment_id;
    ScriptVariable cmt_tree_dir, page_source, page_html_file;
    ScriptVariable html_start_mark, html_end_mark;
    ScriptVariable page_url, orig_url;
    ScriptVariable premodq_page_id;
    ScriptVariable access;
    int recent_timeout;
};


/*  DiscussDisplayData represents data obtained from the generator's
    database and/or from the discussion page's HTML code.
    The get_discuss_display_data allows us NOT to have the generator's
    database code within the cgi program.
    NB: 'hidden' means hidden comment, not a page; for a hidden page,
    get_discuss_display_data returns false, as if the page didn't
    exist.  This is because no permissions allow the user to view
    hidden pages (in contrast to hidden comments).
 */

struct DiscussDisplayData {
    bool comments_enabled, toplevel, hidden;
    ScriptVariable title, body, bodysrc, user_id, user_name;
    int parent_id;  // -1 if not a comment, 0 if top-level comment
    ScriptVector flags;
    long long unixtime;
};

bool get_discuss_display_data(const char *target_encoding,
                              const char *allowed_tags,
                              const char *allowed_attrs,
                              const DiscussionInfo &src,
                              DiscussDisplayData &result);


struct NewCommentData {
    ScriptVariable user_id, user_name, title, body;
    int parent_comment_id;
    ScriptVariable creator_addr, creator_session, creator_date;
};

void get_preview_data(const char *encd, const char *tags, const char *attrs,
                      const NewCommentData &cmt_data,
                      ScriptVariable &username,
                      ScriptVariable &title, ScriptVariable &body);

int save_new_comment(const ScriptVariable &cmtdir,
                     const NewCommentData &cmt_data,
                     ScriptVariable *cmt_file);


/* ``raw'' functions for comment editing */

class HeadedTextMessage;

bool get_comment(const DiscussionInfo &src, HeadedTextMessage &result);
bool save_comment(const DiscussionInfo &src, const HeadedTextMessage &result);
bool delete_comment(const DiscussionInfo &src);

void get_owner_and_date_from_htm(const HeadedTextMessage &htm,
                   ScriptVariable &cmt_owner, long long &cmt_unixtime);

void replace_comment_content(HeadedTextMessage &comment_file,
                const ScriptVariable &subject, const ScriptVariable &body,
                const ScriptVariable &logmark);


bool htm_has_flag(const HeadedTextMessage &hm, const char *flag);

bool get_comment_list(ScriptVariable dir, ScriptVariable subdir,
                      ScriptVector &result);

bool get_comment_hdr_by_path(ScriptVariable dir,
                             ScriptVariable subdir, int id,
                             HeadedTextMessage &result);

void get_encoded_fields_from_hm(const HeadedTextMessage &hm,
                                const char *target_encoding,
                                const char *allowed_tags,
                                const char *allowed_attrs,
                                ScriptVariable &title,
                                ScriptVariable &username);

#endif
