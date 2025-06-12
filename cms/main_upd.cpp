#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>
#include <scriptpp/scrmsg.hpp>
#include <scriptpp/cmd.hpp>


#include "database.hpp"
#include "main_all.hpp"

#include "main_upd.hpp"

#define FILESPEC_TEXT\
    "The <filespec> may be either a file path or a Thalassa object\n"\
    "specification.  In case the argument contains at least one ``/'',\n"\
    "it is taken as a path to an existing file; the Thalassa database\n"\
    "is not even loaded in this case.  If the argument contains at\n"\
    "least one ``='', it is considered to be a Thalassa object\n"\
    "specification, which must either specify a pageset page ID,\n"\
    "or a comment ID for either a list item page, or a pageset page.\n"\
    "In case there's neither a ``/'' nor a ``='' in the argument\n"\
    "(which is not recommended, it is better to use ./name for files\n"\
    "and setID=pgID for pageset pages), Thalassa will first try a\n"\
    "file in the current working directory, and if there's no such\n"\
    "file or it is not a regular file, will try to load the database\n"\
    "and use the given name as a pageset page ID (which only works if\n"\
    "you've got exactly one pageset).\n"\
    "\n"\
    "Thalassa object specifications have the form\n"\
    "\n"\
    "    [<type>=[<realmID>=]]<pageID>[=<commentID>]\n"\
    "\n"\
    "where <type> is one of ``list'', ``set'' or ``pageset'' (the\n"\
    "latter two are equal), but is in most cases simply omitted;\n"\
    "<realmID> is an ID of either a list or a pageset;\n"\
    "<pageID> is either a pageset page ID, or an item ID within\n"\
    "a list with item pages;\n"\
    "<commentID> is, well, the comment ID.\n"\
    "Please note that in case the <commentID> is omitted, then the\n"\
    "type must not be ``list'' and the realmID must not be a list ID\n"\
    "because list item pages don't have source files, and this command\n"\
    "is only intended to work with headed text files.  The <realmID> may\n"\
    "only be omitted in case you only have one realm: either exactly one\n"\
    "pageset and no lists, or no pagesets and exactly one list.  If you\n"\
    "specify a pair like XXX=YYY, it is considered to be pageID=commentID\n"\
    "if and only if you have exactly one realm, otherwise it is\n"\
    "interpreted as setID=pageID.\n"


void help_update(FILE *stream)
{
    fprintf(stream,
        "The ``update'' command updates a ``headed text'' (page/comment) file."
                                                                           "\n"
        "Usage:\n"
        "\n"
        "    thalassa [...] update <filespec> [<options>]\n"
        "\n"
        FILESPEC_TEXT
        "\n"
        "Valid options are:\n"
        "\n"
        "    -n             dry run: show what would be done but don't do\n"
        "    +T <tag>       add the tag\n"
        "    -T <tag>       remove the tag\n"
        "    +F <flag>      add the flag\n"
        "    -F <flag>      remove the flag\n"
        "    +L <label>     add the label\n"
        "    -L <label>     remove the label\n"
        "    -D <unixtime>  use the <unixtime> instead of the current time\n"
        "    -d             force updating the ``unixtime:'' field:\n"
        "                     update even if it is already there\n"
        "    -t             update the ``teaser_len:'' field: if there's no\n"
        "                     ``descr:'' field, search the body for the\n"
        "                     ``<!--break-->'' string and put its position\n"
        "                     into the field, or put the length body if the\n"
        "                     string isn't found; if the ``descr:'' field\n"
        "                     is there, just remove the ``teaser_len:''\n"
        "    -i             suppress updating of the ``id:'' field\n"
        "                     by default, it is set accordingly to the\n"
        "                     object specification or file path\n"
        "    -b <suffix>    set the backup file suffix (default: ``~'')\n"
        "    -B             don't create backup files\n"
        "    -v             verbose messages\n"
        "\n"
    );
}

enum file_types { ft_guess, ft_page, ft_comment };

struct file_choice {
    ScriptVariableInv filename, id;
    int file_type;
    file_choice() : file_type(ft_guess) {}
};

enum task_categories { ct_label, ct_flag, ct_tag };

struct Task {
    int category;
    bool add;   // false to remove
    ScriptVariable name;
    Task *next;
    Task(int c, bool a, const ScriptVariable &n)
        : category(c), add(a), name(n), next(0) {}
};

struct cmdline_update : public file_choice {
    Task *first, *last;
    bool dry_run;
    ScriptVariable baksuffix;
    bool backup;
    long long date;
    bool force_date_update;
    bool teaser_update;
    bool dont_touch_id;
    bool verbose;
    cmdline_update()
        : first(0), last(0), dry_run(false),
        baksuffix("~"), backup(true), date(0), force_date_update(false),
        teaser_update(false), dont_touch_id(false), verbose(false)
    {}
    ~cmdline_update();
    void AddTask(int c, bool a, const ScriptVariable &n);
};

cmdline_update::~cmdline_update()
{
    while(first) {
        Task *tmp = first;
        first = first->next;
        delete tmp;
    }
}

void cmdline_update::AddTask(int c, bool a, const ScriptVariable &n)
{
    Task *p = new Task(c, a, n);
    if(last)
        last->next = p;
    else
        first = p;
    last = p;
}

const char *headername(int c)
{
    switch(c) {
    case ct_label: return "label";
    case ct_flag:  return "flags";
    case ct_tag:   return "tags";
    }
    // we shouln't have reached this point!
    fprintf(stderr, "BUG: unknown header code\n");
    exit(5);
}

static int action_by_char(char c)
{
    switch(c) {
    case 'L':   return ct_label;
    case 'F':   return ct_flag;
    case 'T':   return ct_tag;
    default:    return -1;   /* shouldn't happen */
    }
}


    // the file name (or object spec) is already not there
static bool
parse_update_cmdl(int argc, const char *const *argv, cmdline_update &options)
{
    const char *const *p = argv;
    while(*p) {
        if(((*p)[0] == '+' || (*p)[0] == '-') &&
            ((*p)[1] == 'T' || (*p)[1] == 'F' || (*p)[1] == 'L') &&
            ((*p)[2] == 0))
        {
            if(!p[1]) {
                fprintf(stderr, "action requires a parameter\n");
                return false;
            }
            options.AddTask(action_by_char((*p)[1]), (*p)[0] == '+', p[1]);
            p += 2;
            continue;
        }
        if((*p)[0] == '-') {
            if((*p)[1] == 0 || (*p)[2] != 0) {
                fprintf(stderr, "option %s unknown\n", *p);
                return false;
            }
            switch((*p)[1]) {
            case 'b':
                if(!p[1]) {
                    fprintf(stderr, "-b requires a parameter\n");
                    return false;
                }
                options.baksuffix = p[1];
                p += 2;
                break;
            case 'D':
                if(!p[1]) {
                    fprintf(stderr, "-D requires a parameter\n");
                    return false;
                }
                {   // to localize vars
                    long long d;
                    bool ok = ScriptVariable(p[1]).GetLongLong(d, 10);
                    if(!ok || d < 1) {
                        fprintf(stderr, "invalid date ``%s''\n", p[1]);
                        return false;
                    }
                    options.date = d;
                }
                p += 2;
                break;
            case 'B':
                options.backup = false;
                p++;
                break;
            case 'n':
                options.dry_run = true;
                p++;
                break;
            case 'd':
                options.force_date_update = true;
                p++;
                break;
            case 't':
                options.teaser_update = true;
                p++;
                break;
            case 'i':
                options.dont_touch_id = true;
                p++;
                break;
            case 'v':
                options.verbose = true;
                p++;
                break;
            default:
                fprintf(stderr, "option %s unknown\n", *p);
                return false;
            }
            continue;
        }
        if((*p)[0] == '+') {
            fprintf(stderr, "action %s unknown\n", *p);
            return false;
        }
        fprintf(stderr, "extra argument(s) starting with %s\n", *p);
        return false;
    }
    return true;
}

//////////////////////////////////////////////////////////////////////

static bool svec_has_elem(const ScriptVector &v, const ScriptVariable &el)
{
    int i;
    for(i = 0; i < v.Length(); i++) {
        if(v[i] == el)
            return true;
    }
    return false;
}

static bool get_fname_setitem(Database &dbs,
                 const ScriptVariable &set_id, const ScriptVariable &page_id,
                 file_choice &choice)
{
    const char *sid = set_id.c_str();
    bool ok;
    PageSetData sd;
    ok = dbs.GetSetData(set_id, sd);
    if(!ok) {
        fprintf(stderr, "%s: no such pageset or can't load\n", sid);
        return false;
    }
    ScriptVariable srcd, srcf;
    bool separdir;
    ok = dbs.GetSetItemSource(set_id, page_id, srcd, srcf, separdir);
    if(!ok) {
        fprintf(stderr, "%s=%s: can't find pageset item source file\n",
                        sid, page_id.c_str());
        return false;
    }
    choice.filename = srcf;
    choice.file_type = ft_page;
    choice.id = page_id;
    return true;
}

static bool get_fname_listitem(Database &dbs,
                 const ScriptVariable &list_id, const ScriptVariable &page_id,
                 file_choice &choice)
{
    ListData ld;
    const char *lid = list_id.c_str();
    bool ok = dbs.GetListData(list_id, ld);
    if(!ok) {
        fprintf(stderr, "%s: no such list or can't load\n", lid);
        return false;
    }
    if(ld.srctype != listsrc_set) {
        fprintf(stderr,
                "%s: list is not set-based, can't update its items\n", lid);
        return false;
    }
    return get_fname_setitem(dbs, ld.srcname, page_id, choice);
}

static bool get_fname_li(Database &dbs, const ScriptVariable &comconf,
                         ListItemData &li, ScriptVariable cmt_id,
                         file_choice &choice)
{
    dbs.SetMacroData(0, &li);
    ScriptVariable path = dbs.GetCommentsPath(comconf);
    dbs.ForgetMacroData();
    if(path.IsInvalid())
        return false;
    path.Trim();
    while(cmt_id.Length() < 4)
        cmt_id.Range(0, 0).Replace("0");
    if(path.Range(-1, 1)[0] != '/')
        path += "/";
    choice.filename = path + cmt_id;
    choice.file_type = ft_comment;
    choice.id = cmt_id;
    return true;
}

static bool get_fname_setcmt(Database &dbs, const ScriptVariable &set_id,
                 const ScriptVariable &page_id, const ScriptVariable &cmt_id,
                 file_choice &choice)
{
    const char *sid = set_id.c_str();
    bool ok;
    PageSetData sd;
    ok = dbs.GetSetData(set_id, sd);
    if(!ok) {
        fprintf(stderr, "%s: no such pageset or can't load\n", sid);
        return false;
    }
    if(sd.comments_conf.IsInvalid() || sd.comments_conf == "") {
        fprintf(stderr, "%s: comments not configured for the set\n", sid);
        return false;
    }

    ListItemData li;
    ok = dbs.GetSetItemDataById(set_id, page_id, &li);
    if(!ok) {
        fprintf(stderr, "%s: no such item in the pageset ``%s''\n",
                page_id.c_str(), sid);
        return false;
    }
    return get_fname_li(dbs, sd.comments_conf, li, cmt_id, choice);
}

static bool get_fname_listcmt(Database &dbs, const ScriptVariable&list_id,
                 const ScriptVariable &page_id, const ScriptVariable &cmt_id,
                 file_choice &choice)
{
    const char *lid = list_id.c_str();
    bool ok;
    ListData ld;
    ok = dbs.GetListData(list_id, ld);
    if(!ok) {
        fprintf(stderr, "%s: no such list or can't load\n", lid);
        return false;
    }
    if(ld.srctype == listsrc_set)
        return get_fname_setcmt(dbs, ld.srcname, page_id, cmt_id, choice);
    if(!ld.pages) {
        fprintf(stderr, "%s: list has no item pages\n", lid);
        return false;
    }
    if(ld.comments_conf.IsInvalid() || ld.comments_conf == "") {
        fprintf(stderr, "%s: comments not configured for the list\n", lid);
        return false;
    }

    int idx = -1;
    int max = ld.items.Length();
    int i;
    for(i = 0; i < max; i++) {
        if(ld.items[i] == page_id) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        fprintf(stderr, "%s: no such item in the list ``%s''\n",
                page_id.c_str(), lid);
        return false;
    }
    ListItemData li;
    ok = dbs.GetListItemData(ld, idx, li);
    if(!ok) {
        fprintf(stderr, "%s=%s: couldn't load the list item data\n",
                lid, page_id.c_str());
        return false;
    }
    return get_fname_li(dbs, ld.comments_conf, li, cmt_id, choice);
}

static ScriptVariable guess_id_from_fname(const ScriptVariable &fname)
{
    ScriptVector pparts(fname, "/", "");
    ScriptVariable last = pparts[pparts.Length()-1];
    if(last == PAGESET_MAIN_FILE_NAME)
        return pparts[pparts.Length()-2];
    if(last.Length() == 4) {
        long n;
        bool num = last.GetLong(n, 10);
        if(num)
           return ScriptNumber(num);
    }
    if(last == "" || last == "." || last == "..")
        return ScriptVariableInv();
    return last;
}

static bool choose_filename(cmdline_common &cmd_com, const char *name_str,
                                             file_choice &choice)
{
    ScriptVariable name(name_str);
    if(name.Strchr('/').IsValid()) {
        choice.filename = name;
        choice.id = guess_id_from_fname(name);
        return true;
    }
    if(name.Strchr('=').IsInvalid()) {   // no ``/'', no ``=''
        FileStat fs(name.c_str());
        if(fs.Exists() && fs.IsRegularFile()) {
            choice.filename = name;
            return true;
        }
    }
    Database database;
    bool ok;
    ok = load_inifiles(database, cmd_com.inifiles, cmd_com.opt_selector);
    if(!ok)
        return false;
    ScriptVector vect(name, "=", "");
    int vlen = vect.Length();
    if(vlen < 1)       // nonsense, but technically possible (empty param)
        return false;
    if(vlen == 1) {    // very special case, partially handled above
        // it must be a pageset page ID, and there must be only one pageset
        ScriptVector sets;
        database.GetSets(sets);
        if(sets.Length() < 1) {
            fprintf(stderr, "there are no page sets\n");
            return false;
        }
        if(sets.Length() > 1) {
            fprintf(stderr,
                    "there are more than one page sets, specify the ID\n");
            return false;
        }
        return get_fname_setitem(database, sets[0], vect[0], choice);
    }
    if(vlen == 2) {   // setID=pageID or pageID=cmtID (if there's 1 realm)
        ScriptVector sets, lists;
        database.GetSets(sets);
        database.GetLists(lists);
        // check if there's one realm
        if(sets.Length() == 0 && lists.Length() == 1)
            return get_fname_listcmt(database, lists[0], vect[0], vect[1],
                                     choice);
        if(sets.Length() == 1 && lists.Length() == 0)
            return get_fname_setcmt(database, sets[0], vect[0], vect[1],
                                     choice);
        // more than one realm (or none at all? n/p, will fail later)
        return get_fname_setitem(database, vect[0], vect[1], choice);
    }
    // 3, 4
    if(vect[0] == "list") {
        if(vlen == 3)
            return get_fname_listitem(database, vect[1], vect[2], choice);
        else
            return get_fname_listcmt(database, vect[1], vect[2], vect[3],
                                     choice);
    }
    if(vect[0] == "set" || vect[0] == "pageset") {
        if(vlen == 3)
            return get_fname_setitem(database, vect[1], vect[2], choice);
        else
            return get_fname_setcmt(database, vect[1], vect[2], vect[3],
                                    choice);
    }
    if(vlen > 3) {
        fprintf(stderr, "realm type ``%s'' unknown\n", vect[0].c_str());
        return false;
    }
    // exactly 3 tokens, must be realmID=pageID=cmtID
    // now we need to guess the realm type
    ScriptVector sets, lists;
    database.GetSets(sets);
    database.GetLists(lists);
    bool may_be_set = svec_has_elem(sets, vect[0]);
    bool may_be_list = svec_has_elem(lists, vect[0]);
    if(may_be_list && may_be_set) {
        fprintf(stderr,
                "both list and set named ``%s'' exist, give the realm type\n",
                vect[0].c_str());
        return false;
    }
    if(may_be_list)
        return get_fname_listcmt(database, vect[0], vect[1], vect[2], choice);
    if(may_be_set)
        return get_fname_setcmt(database, vect[0], vect[1], vect[2], choice);
    // neither?
    fprintf(stderr, "there's no list nor set named ``%s''\n", vect[0].c_str());
    return false;
}

//////////////////////////////////////////////////////////////////////

static void print_headers(const HeadedTextMessage &parser, FILE *stream)
{
    const ScriptVector &h = parser.GetHeaders();
    int len = h.Length();
    int i;
    for(i = 0; i < len; i += 2)
        fprintf(stream, "%s: %s\n", h[i].c_str(), h[i+1].c_str());
}

//////////////////////////////////////////////////////////////////////

static void perform_single_task(Task *task, HeadedTextMessage &parser,
                                const cmdline_update &opts)
{
    const char *hname = headername(task->category);
    ScriptVector &hdr = parser.GetHeaders();
    bool found = false;
    int i;
    for(i = 0; i < hdr.Length(); i += 2)
        if(hdr[i] == hname) {
            found = true;
            break;
        }
    if(!found) {  // trivial case
        if(task->add) {
            hdr.AddItem(hname);
            hdr.AddItem(task->name);
        } else {
            // actually, nothing to do: can't remove anything
            // from an empty list
        }
        return;
    }
    ScriptVector v(hdr[i+1], ",", " \t\r");
    int pos = -1;
    int j;
    for(j = 0; j < v.Length(); j++)
        if(v[j] == task->name) {
            pos = j;
            break;
        }

    if(task->add) {
        if(pos != -1)   // already there!
            return;
        v.AddItem(task->name);
    } else {
        if(pos == -1)   // not there, can't remove
            return;
        v.Remove(pos, 1);
    }
    // if we're here, we should reconstruct the changed header
    if(v.Length() == 0)   // empty? simply remove the header altogether
        hdr.Remove(i, 2);
    else
        hdr[i+1] = v.Join(", ");
}

static void
do_update_unixtime(const cmdline_update &opts, HeadedTextMessage &parser)
{
    if(!opts.force_date_update) {
        ScriptVariable orig = parser.FindHeader("unixtime");
        if(orig.IsValid() && orig != "")
            return;
    }
    long long tm = opts.date > 1 ? opts.date : time(0);
    parser.SetHeader("unixtime", ScriptNumber(tm));
}

static void do_update_teaser(HeadedTextMessage &parser)
{
    ScriptVariable descr = parser.FindHeader("descr");
    if(descr.IsValid() && descr != "") {
        parser.RemoveHeader("teaser_len");
        return;
    }
    ScriptVariable &body = parser.GetBody();
    ScriptVariable::Substring pos = body.Strstr("<!--break-->");
    int x = pos.IsValid() ? pos.Index() : body.Length();
    parser.SetHeader("teaser_len", ScriptNumber(x));
}

static bool proceed_update_headedtext(const cmdline_update &opts)
{
    const char *fp = opts.filename.c_str();
    FILE *s = fopen(fp, "r");
    if(!s) {
        perror(fp);
        return false;
    }
    HeadedTextMessage parser;
    int c;
    while((c = fgetc(s)) != EOF) {
        if(!parser.FeedChar(c))   // int, ok
            break;
    }
    fclose(s);

    if(opts.dry_run || opts.verbose) {
        fprintf(stderr, "== header fields before changes ==============\n");
        print_headers(parser, stderr);
    }

    do_update_unixtime(opts, parser);
    if(!opts.dont_touch_id) {
        if(opts.id.IsValid() && opts.id != "")
            parser.SetHeader("id", opts.id);
        else
            fprintf(stderr, "WARNING: couldn't guess the id, not updated\n");
    }
    if(opts.teaser_update)
        do_update_teaser(parser);

    Task *p;
    for(p = opts.first; p; p = p->next)
        perform_single_task(p, parser, opts);

    if(opts.dry_run || opts.verbose) {
        fprintf(stderr, "== header fields after changes ===============\n");
        print_headers(parser, stderr);
        fprintf(stderr, "==============================================\n");
    }
    if(opts.dry_run) {
        fprintf(stderr, "NOTICE: dry run, not updating any files\n");
        return true;
    }

    if(opts.backup) {
        int r = rename(fp, (opts.filename + opts.baksuffix).c_str());
        if(r == -1) {
            perror((opts.filename + opts.baksuffix).c_str());
            fprintf(stderr, "couldn't backup old version, aborting\n");
            return false;
        }
    }

    // save parser to file
    s = fopen(fp, "w");
    if(!s) {
        perror(fp);
        return false;
    }
    ScriptVariable nc = parser.Serialize();
    fputs(nc.c_str(), s);
    fclose(s);
    return true;
}

//////////////////////////////////////////////////////////////////////

int perform_update(cmdline_common &cmd_com, int argc, const char * const *argv)
{
    if(argc < 2) {
        fprintf(stderr, "``update'' requires args, try ``help update''\n");
        return 1;
    }

    cmdline_update opts;
    bool ok;
    ok = choose_filename(cmd_com, argv[1], opts);
    if(!ok)
        return 1;
    ok = parse_update_cmdl(argc - 2, argv + 2, opts);
    if(!ok)
        return 1;
    if(opts.dry_run || opts.verbose)
        fprintf(stderr, "NOTICE: file '%s' selected\n", opts.filename.c_str());
    ok = proceed_update_headedtext(opts);
    if(!ok)
        return 1;
    return 0;
}



//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void help_inspect(FILE *stream)
{
    fprintf(stream,
        "The ``inspect'' command prints a ``headed text'' file header\n"
        "and optionally some lines of its body.  Usage:\n"
        "\n"
        "    thalassa [...] inspect <filespec> [<options>]\n"
        "\n"
        FILESPEC_TEXT
        "\n"
        "Valid options are:\n"
        "\n"
        "    -b <n>         show the first <n> lines of the body\n"
        "    -B             show the whole body\n"
        "    -v             verbose messages\n"
        "\n"
    );
}

struct cmdline_inspect : public file_choice {
    int lines;
    bool whole_body, verbose;
    cmdline_inspect() : lines(0), whole_body(false), verbose(false) {}
};

    // the file name (or object spec) is already not there
static bool
parse_inspect_cmdl(int argc, const char *const *argv, cmdline_inspect &options)
{
    const char *const *p = argv;
    while(*p) {
        if((*p)[0] == '-') {
            if((*p)[1] == 0 || (*p)[2] != 0) {
                fprintf(stderr, "option %s unknown\n", *p);
                return false;
            }
            switch((*p)[1]) {
            case 'b':
                if(!p[1]) {
                    fprintf(stderr, "-b requires a parameter\n");
                    return false;
                }
                {   // to localize vars
                    long n;
                    bool ok = ScriptVariable(p[1]).GetLong(n, 10);
                    if(!ok || n < 0) {
                        fprintf(stderr, "invalid number ``%s''\n", p[1]);
                        return false;
                    }
                    options.lines = n;
                }
                p += 2;
                break;
            case 'B':
                options.whole_body = true;
                p++;
                break;
            case 'v':
                options.verbose = true;
                p++;
                break;
            default:
                fprintf(stderr, "option %s unknown\n", *p);
                return false;
            }
            continue;
        }
        fprintf(stderr, "extra argument(s) starting with %s\n", *p);
        return false;
    }
    return true;
}

int perform_inspect(cmdline_common &cmd_com, int argc, const char *const *argv)
{
    if(argc < 2) {
        fprintf(stderr, "``inspect'' requires args, try ``help inspect''\n");
        return 1;
    }

    cmdline_inspect options;
    bool ok;
    ok = choose_filename(cmd_com, argv[1], options);
    if(!ok)
        return 1;
    ok = parse_inspect_cmdl(argc - 2, argv + 2, options);
    if(!ok)
        return 1;
    if(options.verbose)
        fprintf(stderr, "NOTICE: file '%s' selected\n",
                        options.filename.c_str());

    const char *fp = options.filename.c_str();
    FILE *s = fopen(fp, "r");
    if(!s) {
        perror(fp);
        return false;
    }
    HeadedTextMessage parser;
    int c;
    while((c = fgetc(s)) != EOF) {
        if(!parser.FeedChar(c))   // int, ok
            break;
    }
    fclose(s);

    print_headers(parser, stdout);
    fputc('\n', stdout);

    int lines = 0;
    const char *b = parser.GetBody().c_str();
    while(*b && (lines < options.lines || options.whole_body)) {
        fputc(*b, stdout);
        if(*b == '\n')
            lines++;
        b++;
    }

    return 0;
}
