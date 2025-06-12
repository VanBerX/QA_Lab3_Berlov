#include <stdio.h>

#include "database.hpp"
#include "main_all.hpp"
#include "main_lst.hpp"



void help_list(FILE *stream)
{
    fprintf(stream,
        "THE ``list'' COMMAND IS PARTIALLY UNIMPLEMENTED! DON'T EXPECT MUCH\n"
        "\n"
        "The ``list'' command lists configured/existing objects.  Usage:\n"
        "\n"
        "    thalassa [...] list [options] [<type> [<id> [<item> [<cmt_id>]]]]"
                                                                          "\n"
        "\n"
        "where options are:\n"
        "\n"
        "    -l             (ell) long output format\n"
        "    -a             list all _under_ the specified level\n"
        "    -m             show coMmentary-capable objects only\n"
        "\n"
        "Currently the following target types are recognized:\n"
        "   list, set, page, collection, genfile, binary, aliases\n"
        "Also ``pageset'' is recognized as an alias for ``set'', and\n"
        "   ``bin'' may be used instead of ``binary''.\n"
    );
}

enum object_type {
    ot_unknown = 0,
    ot_list, ot_pageset, ot_page, ot_collection, ot_genfile,
    ot_binary, ot_aliases
};

static int object_type_from_string(ScriptVariable s)
{
    s.Tolower();
    s.Trim();
    if(s == "list")
        return ot_list;
    if(s == "set" || s == "pageset")
        return ot_pageset;
    if(s == "page")
        return ot_page;
    if(s == "collection")
        return ot_collection;
    if(s == "genfile")
        return ot_genfile;
    if(s == "binary")
        return ot_binary;
    if(s == "aliases")
        return ot_aliases;
    return ot_unknown;
}

struct cmdline_list {
    bool long_format, list_all, comments_only;
    int objtype;
    ScriptVariableInv obj_id, item_id, cmt_id;
    cmdline_list()
        : long_format(false), list_all(false), comments_only(false),
        objtype(ot_unknown) {}
};

static int type_can_have_comments(int t)
{
    switch(t) {
    case ot_unknown:
    case ot_list:
    case ot_pageset:
        return true;
    default:
        return false;
    }
}

static bool parse_list_cmdl(cmdline_list &cmdl, const char * const *argv)
{
    ScriptVector ids;
    int c = 1;
    while(argv[c]) {
        ScriptVariable a(argv[c]);
        if(a == "-l") {
            cmdl.long_format = true;
            c++;
        } else
        if(a == "-a") {
            cmdl.list_all = true;
            c++;
        } else
        if(a == "-m") {
            cmdl.comments_only = true;
            c++;
        } else
        if(a[0] == '-') {
            fprintf(stderr, "option ``%s'' unrecognized\n", argv[c]);
            return false;
        } else { // not a -flag
            ids.AddItem(a);
            if(ids.Length() > 4) {
                fprintf(stderr, "too many IDs (for ``list'' command)\n");
                return false;
            }
            c++;
        }
    }
    int ilen = ids.Length();
    if(ilen > 3)
        cmdl.cmt_id = ids[3];
    if(ilen > 2)
        cmdl.item_id = ids[2];
    if(ilen > 1)
        cmdl.obj_id = ids[1];
    if(ilen > 0) {
        int t = object_type_from_string(ids[0]);
        if(t == ot_unknown) {
            fprintf(stderr, "unknown object type ``%s''\n", ids[0].c_str());
            return false;
        }
        cmdl.objtype = t;
    }
    if(cmdl.comments_only && !type_can_have_comments(cmdl.objtype)) {
        fprintf(stderr, "only lists and sets can have comments\n");
        return false;
    }
    return true;
}

static bool str_ok(const ScriptVariable &s)
{
    return s.IsValid() && s != "";
}

static bool no_str(const ScriptVariable &s)
{
    return s.IsInvalid() || s == "";
}

static void print_object_header(const cmdline_list &cmdl,
                                const char *ot, const char *id)
{
    ScriptVariable s;
    if(cmdl.objtype == ot_unknown) {
        s = "[";
        s += ot;
        s += ' ';
        s += id;
        s += ']';
    } else {
        s = id;
    }
    while(s.Length() < 23)
        s += ' ';
    fputs(s.c_str(), stdout);
}

static void list_the_list(Database &database, const cmdline_list &cmdl,
                          const ListData &ld)
{
    bool tiny = !cmdl.list_all && !cmdl.long_format;
    bool rev = ld.reverse;
    int i;
    int len = ld.items.Length();
    for(i = rev ? len-1 : 0; rev ? (i >= 0) : (i < len); rev ? i-- : i++) {
        if(tiny) {
            printf("%s\t", ld.items[i].c_str());
            continue;
        }
        printf("%s", ld.items[i].c_str());

        putchar('\n');
    }
    if(tiny)
        putchar('\n');

}

static void list_lists(Database &database, cmdline_list &cmdl)
{
    if(str_ok(cmdl.obj_id)) {
        ListData ld;
        bool ok = database.GetListData(cmdl.obj_id, ld);
        if(!ok)
            return;
        list_the_list(database, cmdl, ld);
        return;
    }
    bool ls_ls = cmdl.list_all;
    cmdl.list_all = false;
    ScriptVector ids;
    database.GetLists(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        ListData ld;
        bool ok = database.GetListData(ids[i], ld);
        if(!ok)
            continue;
        if(cmdl.comments_only && (!ld.pages || no_str(ld.comments_conf)))
            continue;
        print_object_header(cmdl, "list", ids[i].c_str());
        if(cmdl.long_format) {
            printf("\t# (%s %s%s%s)",
                   ld.srctype == listsrc_ini ? "ini" : "set",
                   ld.srcname.c_str(),
                   ld.srctype == listsrc_ini ? "" : " ",
                   ld.srctype == listsrc_ini ? "" : ld.tag.c_str());
            if(ld.embedded)
                fputs(" embedded", stdout);
            fputs(ld.pages ? " pages" : " nopages", stdout);
            if(ld.reverse)
                fputs(" rev", stdout);
            if(ld.pages && !no_str(ld.comments_conf))
                fputs(" comments", stdout);
        }
        fputc('\n', stdout);
        if(ls_ls) {
            list_the_list(database, cmdl, ld);
            fputc('\n', stdout);
        }
    }
}

static void list_sets(Database &database, const cmdline_list &cmdl)
{
    ScriptVector ids;
    database.GetSets(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        PageSetData sd;
        bool ok = database.GetSetData(ids[i], sd);
        if(!ok)
            continue;
        if(cmdl.comments_only && no_str(sd.comments_conf))
            continue;
        print_object_header(cmdl, "pageset", ids[i].c_str());
        if(cmdl.long_format) {
            printf("\t# (%s)", sd.setdirname.c_str());
            if(!no_str(sd.comments_conf))
                fputs(" comments", stdout);
        }
        fputc('\n', stdout);
    }
}

static void list_pages(Database &database, const cmdline_list &cmdl)
{
    ScriptVector ids;
    database.GetPages(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        print_object_header(cmdl, "page", ids[i].c_str());
        if(cmdl.long_format) {
            int um = database.GetPageChmod(ids[i]);
            if(um > 0)
                printf("\t# chmod=%04o", um);
        }
        fputc('\n', stdout);
    }
}

static void list_collections(Database &database, const cmdline_list &cmdl)
{
    ScriptVector ids;
    database.GetCollections(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        print_object_header(cmdl, "collection", ids[i].c_str());
        if(cmdl.long_format) {
            // XXXX
        }
        fputc('\n', stdout);
    }
}

static void list_genfiles(Database &database, const cmdline_list &cmdl)
{
    ScriptVector ids;
    database.GetGenfiles(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        print_object_header(cmdl, "genfile", ids[i].c_str());
        if(cmdl.long_format) {
            // XXXX
        }
        fputc('\n', stdout);
    }
}

static void list_binaries(Database &database, const cmdline_list &cmdl)
{
    ScriptVector ids;
    database.GetBinaries(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        print_object_header(cmdl, "binary", ids[i].c_str());
        if(cmdl.long_format) {
            // XXXX
        }
        fputc('\n', stdout);
    }
}

static void list_aliases(Database &database, const cmdline_list &cmdl)
{
    ScriptVector ids;
    database.GetAliasSections(ids);
    int i;
    for(i = 0; i < ids.Length(); i++) {
        print_object_header(cmdl, "aliases", ids[i].c_str());
        if(cmdl.long_format) {
            // XXXX
        }
        fputc('\n', stdout);
    }
}

static void list_really_all(Database &database, cmdline_list &cmdl)
{
    cmdl.list_all = false;
    list_lists(database, cmdl);
    list_sets(database, cmdl);
    if(cmdl.comments_only)
        return;
    list_pages(database, cmdl);
    list_collections(database, cmdl);
    list_genfiles(database, cmdl);
    list_binaries(database, cmdl);
    list_aliases(database, cmdl);
}

int perform_list(cmdline_common &cmd_com, int argc, const char * const *argv)
{
    cmdline_list cmdl;
    bool ok;

    ok = parse_list_cmdl(cmdl, argv);
    if(!ok)
        return 1;

    Database database;
    if(cmdl.objtype != ot_unknown || cmdl.list_all) {
        ok = load_inifiles(database, cmd_com.inifiles, cmd_com.opt_selector);
        if(!ok)
            return 1;
    }

    fprintf(stderr, "WARNING! THE COMMAND IS PARTIALLY UNIMPLEMENTED\n\n");

    switch(cmdl.objtype) {
    case ot_unknown:
        if(cmdl.list_all) {
            list_really_all(database, cmdl);
        } else {
            fputs(
                cmdl.long_format ?
                    "list\nset\npage\ncollection\ngenfile\nbinary\naliases\n"
                    : "list set page collection genfile binary aliases\n",
                stdout
            );
        }
        break;
    case ot_list:
        list_lists(database, cmdl);
        break;
    case ot_pageset:
        list_sets(database, cmdl);
        break;
    case ot_page:
        list_pages(database, cmdl);
        break;
    case ot_collection:
        list_collections(database, cmdl);
        break;
    case ot_genfile:
        list_genfiles(database, cmdl);
        break;
    case ot_binary:
        list_binaries(database, cmdl);
        break;
    case ot_aliases:
        fprintf(stderr, "not implemented yet\n");
        return 1;
    }
    return 0;
}
