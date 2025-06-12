#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrmsg.hpp>
#include <scriptpp/cmd.hpp>

#include "filters.hpp"
#include "fileops.hpp"
#include "fpublish.hpp"

#include "tcgi_rpl.hpp"


#define COMMENT_SAVING_FORMAT "tags, texbreaks"


static bool read_headed_text(ScriptVariable fname, HeadedTextMessage &parser,
                             bool read_body)
{
#if 0
    FILE *f = fopen(fname.c_str(), "r");
    if(!f)
        return false;
    int c;
    while((c = fgetc(f)) != EOF) {
        if(!parser.FeedChar(c)) {   // int, ok, but commented out anyway
            fclose(f);
            return false;
        }
        if(!read_body && parser.InBody())
            break;
    }
    fclose(f);
    return true;
#endif

    if(fname.IsInvalid() || fname == "")
        return false;

    static unsigned char buf[4096];    // unsignedness is critical here!
    int fd, rc;
    fd = open(fname.c_str(), O_RDONLY);
    if(fd == -1)
        return false;
    bool res = true;
    while((rc = read(fd, buf, sizeof(buf))) > 0) {
        int i;
        for(i = 0; i < rc; i++) {
            if(!parser.FeedChar(buf[i])) {   // fixed by unsignedness
                res = false;
                goto quit;
            }
            if(!read_body && parser.InBody())
                break;
        }
    }
quit:
    close(fd);
    return res;
}

bool htm_has_flag(const HeadedTextMessage &hm, const char *flag)
{
    ScriptVariable fls = hm.FindHeader("flags");
    if(fls.IsInvalid() || fls == "")
        return false;
    ScriptVector vf(fls, ",", " \t\r\n");
    int i;
    for(i = 0; i < vf.Length(); i++)
        if(vf[i] == flag)
            return true;
    return false;
}

void get_owner_and_date_from_htm(const HeadedTextMessage &htm,
                   ScriptVariable &cmt_owner, long long &cmt_unixtime)
{
    cmt_owner = htm.FindHeader("user");

    cmt_unixtime = 0;
    ScriptVariable tmp = htm.FindHeader("unixtime");
    if(tmp.IsValid()) {
        long long ut;
        bool ok = tmp.GetLongLong(ut, 10);
        if(ok)
            cmt_unixtime = ut;
    }
}

static void get_data_from_hm(const HeadedTextMessage &hm,
                             const FilterChainMaker &filt_maker,
                             DiscussDisplayData &result,
                             bool get_body)
{
    const ScriptVector &hdr = hm.GetHeaders();
    FilterChainSet *fcs = filt_maker.MakeChainSet(hdr);

    get_owner_and_date_from_htm(hm, result.user_id, result.unixtime);

    ScriptVariable tmp;
    ScriptVariableInv inv;

    tmp = hm.FindHeader("title");
    result.title = tmp.IsValid() ? fcs->ConvertUserdata(tmp) : inv;
    tmp = hm.FindHeader("from");
    result.user_name = tmp.IsValid() ? fcs->ConvertUserdata(tmp) : inv;

    tmp = hm.FindHeader("parent");
    if(tmp.IsInvalid() || tmp.Trim() == "") {
        result.parent_id = -1;
    } else {
        long n;
        bool ok = tmp.GetLong(n, 10);
        result.parent_id = ok ? n : -1;
    }

    tmp = hm.FindHeader("flags");
    if(tmp.IsValid())
        result.flags = ScriptVector(tmp, ",", " \t\r\n");

    if(get_body) {
        result.body = fcs->ConvertContent(hm.GetBody());
        result.bodysrc = fcs->ConvertEncOnly(hm.GetBody());
    }

    delete fcs;
}

ScriptVariable get_body_from_html(const DiscussionInfo &src)
{
    if(src.page_html_file.IsInvalid() || src.page_html_file == "")
        return ScriptVariableInv();
    ReadText rt(src.page_html_file.c_str());
    if(!rt.IsOpen())
        return ScriptVariableInv();
    ScriptVariable res(""), line;
#if 0
    res = ScriptVariable("<!-- \n") + src.html_start_mark +
          "\n\n" + src.html_end_mark + "\n  -->\n";
#endif
    bool inside = false;
    while(rt.ReadLine(line)) {
        ScriptVariable ln2 = line;
        ln2.Trim();
        if(ln2 == src.html_start_mark)
            inside = true;
        else
        if(ln2 == src.html_end_mark)
            inside = false;
        else {
            if(inside) {
                res += line;
                res += "\n";
            }
        }
    }
    return res;
}

bool get_discuss_display_data(const char *target_encoding,
                              const char *allowed_tags,
                              const char *allowed_attrs,
                              const DiscussionInfo &src,
                              DiscussDisplayData &result)
{
    FilterChainMaker filt_maker(target_encoding, allowed_tags, allowed_attrs);

    // first, just copy the knowledge whether it is toplevel or not
    result.toplevel = (src.comment_id > 0);

    // now determine if the comments are enabled
    ScriptVariable pgs = src.page_source;
    if(pgs.IsInvalid() || pgs.Trim() == "") {
        result.comments_enabled = true;
    } else {
        // okay, so we need to read the original page source
        bool need_pgbody =
            (src.comment_id < 1) &&
            (src.page_html_file.IsInvalid() || src.page_html_file == "");
        HeadedTextMessage orig_page;
        bool ok = read_headed_text(src.page_source, orig_page, need_pgbody);
        if(!ok)
            return false;
        if(htm_has_flag(orig_page, "hidden"))
            return false; // the page itself is hidden, no circumvention!
        ScriptVariable v = orig_page.FindHeader("comments");
        result.comments_enabled = (v.IsValid() && v == "enabled");
        if(src.comment_id < 1) {   // toplevel; we're almost done
            get_data_from_hm(orig_page, filt_maker, result, need_pgbody);
            if(!need_pgbody)  // we need to get it from the html
                result.body = get_body_from_html(src);
            result.hidden = false;
            return result.body.IsValid();
        }
    }
    // we can only get here in case either there's no page source
    // (e.g. it's a list item, not a set item),
    // or we're replying to a comment
    if(src.comment_id > 0) {   // okay, comment
        ScriptVariable fname = src.cmt_tree_dir +
            ScriptVariable(30, "/%04d", src.comment_id);
        HeadedTextMessage orig_comment;
        bool ok = read_headed_text(fname, orig_comment, true);
        if(!ok)
            return false;
        get_data_from_hm(orig_comment, filt_maker, result, true);
        result.hidden = htm_has_flag(orig_comment, "hidden");
        return true;
    }

    // okay, we're replying to a page
    //       AND there's no other source but the html file
    // NB: it can't be hidden, right?
    result.hidden = false;
    result.title.Invalidate();
    result.user_id.Invalidate();
    result.user_name.Invalidate();
    result.parent_id = -1;
    result.body = get_body_from_html(src);
    return result.body.IsValid();
}



void get_preview_data(const char *enc, const char *tags, const char *attrs,
                      const NewCommentData &cmt_data,
                      ScriptVariable &username,
                      ScriptVariable &title, ScriptVariable &body)
{
    FilterChainMaker filt_maker(enc, tags, attrs);
    FilterChainSet *fcs = filt_maker.MakeChainSet("", COMMENT_SAVING_FORMAT);
    username = fcs->ConvertUserdata(cmt_data.user_name);
    title = fcs->ConvertUserdata(cmt_data.title);
    body = fcs->ConvertContent(cmt_data.body);
    delete fcs;
}

//////////////////////////////////////////////////////////////////////
// save comment

    // the name for the file within a comment directory to store the max id
    // please note this macro is also set and used in dbforum.cpp
#ifndef HINT_FNAME
#define HINT_FNAME "_hints"
#endif

    // the absolute maximum for the comment id
#ifndef MAX_COMMENT_ID
#define MAX_COMMENT_ID 50000
#endif

    // how many times we try to pick greater id in case the id
    // is already occupied, despite the information from hints and/or scan
#ifndef CID_RETRIES
#define CID_RETRIES 100
#endif


static int read_hint(const char *hintpath)
{
    int h, r;
    FILE *f;
    f = fopen(hintpath, "r");
    if(!f)
        return -1;
    r = fscanf(f, "%d", &h);
    fclose(f);
    if(r != 1)
        return -1;
    return h;
}

static void write_hint(const char *hintpath, int val)
{
    FILE *f;
    f = fopen(hintpath, "w");
    if(!f)
        return;
    fprintf(f, "%d\n", val);
    fclose(f);
}

static ScriptVariable
serialize_comment(int new_id, const NewCommentData &cmt_data, bool premod)
{
    ScriptVector flags;
    long long unixtime = time(0);
    ScriptVariable content =
        ScriptVariable(1024, "id: %d\n" "unixtime: %lld\n",
                       new_id, unixtime) +
        "format: " COMMENT_SAVING_FORMAT "\n" +
        "from: " + cmt_data.user_name + "\n" +
        "title: " + cmt_data.title + "\n";
    if(cmt_data.parent_comment_id > 0) {
        content += "parent: ";
        content += ScriptNumber(cmt_data.parent_comment_id);
        content += "\n";
    }
    if(cmt_data.user_id.IsValid() && cmt_data.user_id != "") {
        content += "user: ";
        content += cmt_data.user_id;
        content += "\n";
    } else {
        flags.AddItem("anon");
    }
    if(premod) {
        flags.AddItem("hidden");
        flags.AddItem("premod");
    }
    if(flags.Length() > 0) {
        content += "flags: ";
        content += flags.Join(", ");
        content += "\n";
    }
    if(cmt_data.creator_addr.IsValid() && cmt_data.creator_addr != "") {
        content += "creator_addr: ";
        content += cmt_data.creator_addr;
        content += "\n";
    }
    if(cmt_data.creator_session.IsValid() && cmt_data.creator_session != "") {
        content += "creator_session: ";
        content += cmt_data.creator_session;
        content += "\n";
    }
    if(cmt_data.creator_date.IsValid() && cmt_data.creator_date != "") {
        content += "creator_date: ";
        content += cmt_data.creator_date;
        content += "\n";
    }
    content += "\n";
    content += cmt_data.body;
    return content;
}

static int check_and_make_dir(const ScriptVariable &dn)
{
    FileStat dst(dn.c_str());
    if(!dst.Exists()) {
#if 0
        int res = mkdir(dn.c_str(), 0777);
#else
	int res = make_directory_path(dn.c_str(), 0);
#endif
        if(res == -1)
            return -1;
    } else {
        if(!dst.IsDir())
            return -1;
    }
    return 0;
}


int save_new_comment(const ScriptVariable &cmtdir,
                     const NewCommentData &cmt_data,
                     ScriptVariable *cmt_filename)
{
    if(-1 == check_and_make_dir(cmtdir))
        return -1;
    ScriptVariable hintfname = cmtdir + "/" HINT_FNAME;
    int max_id = read_hint(hintfname.c_str());
    if(max_id < 1 || max_id > MAX_COMMENT_ID)    // missing or corrupt
        max_id = 0;

    int fd;
    int new_id = max_id;
    ScriptVariable fname;
    do {
        new_id++;
        if(new_id > MAX_COMMENT_ID)
            return -1;
        fname = cmtdir + ScriptVariable(16, "/%04d", new_id);
        fd = open(fname.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0666);
    } while(fd == -1 && errno == EEXIST);
    if(fd == -1)
        return -1;

    bool premod = cmt_filename != 0;
    ScriptVariable content = serialize_comment(new_id, cmt_data, premod);
    write(fd, content.c_str(), content.Length());
    close(fd);

    write_hint(hintfname.c_str(), new_id);

    if(cmt_filename)
        *cmt_filename = fname;

    return new_id;
}

bool get_comment(const DiscussionInfo &src, HeadedTextMessage &result)
{
    ScriptVariable fname = src.cmt_tree_dir +
        ScriptVariable(30, "/%04d", src.comment_id);
    return read_headed_text(fname, result, true);
}

void replace_comment_content(HeadedTextMessage &comment_file,
                  const ScriptVariable &subject, const ScriptVariable &body,
                  const ScriptVariable &logmark)
{
    comment_file.SetHeader("title", subject);
    comment_file.SetHeader("format", COMMENT_SAVING_FORMAT);
    comment_file.RemoveHeader("encoding");
    if(logmark.IsValid() && logmark != "") {
        ScriptVector &h = comment_file.GetHeaders();
        bool found = false;
        int idx;
        for(idx = 0; idx < h.Length() - 1; idx += 2) {
            if(h[idx] == "cgi_edited") {
                found = true;
                break;
            }
        }
        if(found) {
            h.Insert(idx, "cgi_edited");
            h.Insert(idx+1, logmark);
        } else {
            h.AddItem("cgi_edited");
            h.AddItem(logmark);
        }
    }
    comment_file.SetBody(body);
}

bool save_comment(const DiscussionInfo &src, const HeadedTextMessage &result)
{
    ScriptVariable fname =
        src.cmt_tree_dir + ScriptVariable(16, "/%04d", src.comment_id);
    int fd = open(fname.c_str(), O_WRONLY|O_TRUNC);
    if(fd == -1)
        return false;

    ScriptVariable content = result.Serialize();
    write(fd, content.c_str(), content.Length());
    fsync(fd);
    close(fd);

    return true;
}

bool delete_comment(const DiscussionInfo &src)
{
    ScriptVariable fname =
        src.cmt_tree_dir + ScriptVariable(16, "/%04d", src.comment_id);
    int res = unlink(fname.c_str());
    return res != -1;
}

bool get_comment_list(ScriptVariable dir, ScriptVariable subd,
                      ScriptVector &result)
{
    ScriptVariable fname = dir + "/" + subd;
    ReadDir rdir(fname.c_str());
    if(!rdir.OpenOk())
        return false;
    result.Clear();
    const char *s;
    while((s = rdir.Next())) {
        if(!*s || *s == '.' || *s == '_')
            continue;
        ScriptVariable sv(s);
        long n;
        if(!sv.GetLong(n, 10) || n < 1)
            continue;
        result.AddItem(ScriptNumber(n)); // so leading zeroes are dropped
    }
    return true;
}

bool get_comment_hdr_by_path(ScriptVariable dir, ScriptVariable subd, int id,
                             HeadedTextMessage &result)
{
    ScriptVariable fname = dir + "/" + subd +
        ScriptVariable(30, "/%04d", id);
    return read_headed_text(fname, result, false);
}

void get_encoded_fields_from_hm(const HeadedTextMessage &hm,
                                const char *target_encoding,
                                const char *allowed_tags,
                                const char *allowed_attrs,
                                ScriptVariable &title,
                                ScriptVariable &username)
{
    FilterChainMaker filt_maker(target_encoding, allowed_tags, allowed_attrs);

    const ScriptVector &hdr = hm.GetHeaders();
    FilterChainSet *fcs = filt_maker.MakeChainSet(hdr);

    ScriptVariable tmp;
    ScriptVariableInv inv;

    tmp = hm.FindHeader("title");
    title = tmp.IsValid() ? fcs->ConvertUserdata(tmp) : inv;
    tmp = hm.FindHeader("from");
    username = tmp.IsValid() ? fcs->ConvertUserdata(tmp) : inv;

    delete fcs;
}
