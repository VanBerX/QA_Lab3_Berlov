#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>
#include <scriptpp/cmd.hpp>

#include "main_all.hpp"
#include "main_gen.hpp"

#include "database.hpp"
#include "fileops.hpp"
#include "fpublish.hpp"
#include "generate.hpp"
#include "errlist.hpp"


#ifndef DIRLOCK_FILENAME
#define DIRLOCK_FILENAME "_LOCK"
#endif

void help_gen(FILE *stream)
{
    fprintf(stream,
        "The ``gen'' command is used to generate the site, like this: \n"
        "\n"
        "    thalassa [...] gen <options>\n"
        "\n"
        "where <options> are:\n"
        "\n"
        "    -a             generate everything\n"
        "    -g <targets>   targets to generate (see below)\n"
        "    -r             (r)ebuild: generate everything into a temporary\n"
        "                   dir, then rename the rootdir to have a suffix\n"
        "                   (.1, .2, ...) and rename the temporary dir to\n"
        "                   be the new rootdir;\n"
        "    -s             use the spool directory and locking (see the\n"
        "                   documentation for details)\n"
        "    -t <dir>       generate (t)o the given dir "
                          /* sic! -> */  "(override [general]/rootdir)\n"
        "\n"
        "For -g, <targets> may be a comma- and/or space-separated list\n"
        "(be sure to use quotes to make it a single argument if you use\n"
        "spaces), in which every item is a target specification.\n"
        "   Each target specification consists of a target type, target id\n"
        "and (optionally) element spec, separated by ``=''; for example:\n"
        "   ``set=nodes=n35''  means to generate the page ``n35'' defined\n"
        "                      within the ``nodes'' pageset\n"
        "   ``set=blog''       means to generate all pages of the "
                                                      "``blog'' pageset\n"
        "   ``list=videos''    generate the whole list ``videos''\n"
        "                      (both list pages and item pages, if "
                                                            "applicable)\n"
        "   ``list=videos=v1'' only the item page ``v1'' is generated\n"
        "                      (list pages are not generated at all)\n"
        "Target type alone means to generate all targets of the type\n"
        "(e.g. all lists, or all collections).\n"
        "Currently the following target types are supported:\n"
        "   list, set, page, collection, genfile, binary, aliases\n"
        "Also ``pageset'' is recognized as an alias for ``set'', and\n"
        "``bin'' may be used instead of ``binary''.\n"
    );

}

struct GenCmdline {
    bool gen_all, rebuild, spool;
    ScriptVector targets;
    ScriptVariable target_dir;

    GenCmdline() : gen_all(false), rebuild(false), spool(false) {}
};

static int max_target_args(const ScriptVariable &t)
{
    if(t == "page" || t == "bin" || t == "binary" ||
        t == "genfile" || t == "aliases")
    {
        return 2;   // type and id
    }

    if( t == "list" || t == "set" || t == "pageset" || t == "collection")
        return 3;   // type, id and item_id

    return -1;      // unrecognized
}

static bool sv_is_set(const ScriptVariable &s)
{
    return s.IsValid() && s != "";
}

static bool get_gen_targets(const char *arg, ScriptVector &targets)
{
    ScriptWordVector v(arg, ", \t\r\n");
    int i;
    for(i = 0; i < v.Length(); i++) {
        if(!sv_is_set(v[i]))  // shouldn't happen, actually
            continue;
        ScriptVector tg(v[i], "=", " \t\r\n");
        tg[0].Tolower();
        int max = max_target_args(tg[0]);
        if(max < 1) {
            fprintf(stderr, "type ``%s'' not recognized\n", tg[0].c_str());
            return false;
        }
        if(tg.Length() > max) {
            fprintf(stderr, "target ``%s'' overspecified\n", v[i].c_str());
            return false;
        }
        targets.AddItem(tg.Join("="));
    }
    return true;
}

static bool parse_gen_cmdl(int argc, const char *const *argv, GenCmdline &cm)
{
    bool ok;
    int c = 1;
    while(c < argc && argv[c][0] == '-') {
        if(!argv[c][1] || argv[c][2]) {
            fprintf(stderr, "option ``%s'' unrecognized\n", argv[c]);
            return false;
        }
        if(argv[c][1] == 'g' || argv[c][1] == 't')
        {
            if(!argv[c+1] || argv[c+1][0] == '-') {
                fprintf(stderr, "option ``%s'' requires parameter\n", argv[c]);
                return false;
            }
        }
        switch(argv[c][1]) {
        case 'a':
            if(cm.gen_all) {
                fprintf(stderr, "multiple ``-a'' not allowed\n");
                return false;
            }
            if(cm.targets.Length() != 0) {
                fprintf(stderr, "``-a'' incompatible with ``-g''\n");
                return false;
            }
            cm.gen_all = true;
            c++;
            break;
        case 'r':
            if(cm.rebuild) {
                fprintf(stderr, "multiple ``-r'' not allowed\n");
                return false;
            }
            if(cm.targets.Length() != 0) {
                fprintf(stderr, "``-r'' incompatible with ``-g''\n");
                return false;
            }
            cm.rebuild = true;
            c++;
            break;
        case 's':
            if(cm.spool) {
                fprintf(stderr, "multiple ``-s'' not allowed\n");
                return false;
            }
            cm.spool = true;
            c++;
            break;
        case 'g':
            if(cm.gen_all) {
                fprintf(stderr, "``-g'' incompatible with ``-a''\n");
                return false;
            }
            ok = get_gen_targets(argv[c+1], cm.targets);
            if(!ok)
                return false;
            c += 2;
            break;
        case 't':
            cm.target_dir = argv[c+1];
            c += 2;
            break;
        default:
            fprintf(stderr, "unknown option ``%s''\n", argv[c]);
            return false;
        }
    }

    // now the parameters must be over
    if(argv[c]) {
        fprintf(stderr, "trailing parameter ``%s''\n", argv[c]);
        return false;
    }

    if(!cm.gen_all && !cm.rebuild && cm.targets.Length() == 0) {
        fprintf(stderr, "nothing to generate, try ``-a'', ``-r'' or ``-g''\n");
        return false;
    }
    return true;
}

static void spool_the_targets(const ScriptVariable &spooldir,
                              const ScriptVector &targets,
                              ErrorList **err)
{
    int tl = targets.Length();
    if(tl < 1)
        return;
    make_directory_path(spooldir.c_str(), 0);
    ScriptVariable fingerprint =
        ScriptNumber(time(0)) + "=" + ScriptNumber(getpid()) + "\n";
    int i;
    for(i = 0; i < tl; i++) {
        ScriptVariable fname = spooldir + "/" + targets[i];
        int fd = open(fname.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0666);
        if(fd == -1) {
            if(errno == EEXIST) {
                ScriptVariable s("WARNING! can't spool target ");
                s += targets[i];
                s += " (already there)";
                ErrorList::AddError(err, s);
            } else {
                ScriptVariable s("couldn't spool target ");
                s += targets[i];
                s += ": ";
                s += strerror(errno);
            }
            continue;
        }
        write(fd, fingerprint.c_str(), fingerprint.Length());
        close(fd);
    }
}

static bool trylock_spooldir(const ScriptVariable &spooldir)
{
    ScriptVariable fname = spooldir + "/" + DIRLOCK_FILENAME;
    int fd = open(fname.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0666);
    if(fd == -1)
        return false;
    ScriptVariable s(16, "%d\n", getpid());
    write(fd, s.c_str(), s.Length());
    close(fd);
    return true;
}

static void unlock_spooldir(const ScriptVariable &spooldir)
{
    ScriptVariable fname = spooldir + "/" + DIRLOCK_FILENAME;
    unlink(fname.c_str());
}

static void perform_single_target(const ScriptVariable &targ, Database &db,
                                  ErrorList **err)
{
    ScriptTokenVector v(targ, "=");
    ScriptVariable t = v[0];
    t.Trim().Tolower();

    if(t == "genfile") {
        if(sv_is_set(v[1])) {
            generate_page(v[1], db, err);
        } else {
            generate_all_genfiles(db, err);
        }
    } else
    if(t == "page") {
        if(sv_is_set(v[1])) {
            generate_page(v[1], db, err);
        } else {
            generate_all_pages(db, err);
        }
    } else
    if(t == "collection") {
        if(sv_is_set(v[1])) {
            if(sv_is_set(v[2])) {
                ErrorList::AddError(err,
                    "WARN: collection elements not implemented yet");
            }
            publish_collection(v[1], db, err);
        } else {
            publish_all_collections(db, err);
        }
    } else
    if(t == "bin" || t == "binary") {
        if(sv_is_set(v[1])) {
            publish_binary(v[1], db, err);
        } else {
            publish_all_binaries(db, err);
        }
    } else
    if(t == "list") {
        if(sv_is_set(v[1])) {
            if(sv_is_set(v[2])) {
                generate_single_list_item_page(v[1], v[2], db, err);
            } else {
                generate_list(v[1], db, err);
            }
        } else {
            generate_all_lists(db, err);
        }
    } else
    if(t == "set" || t == "pageset") {
        if(sv_is_set(v[1])) {
            if(sv_is_set(v[2])) {
                generate_single_set_page(v[1], v[2], db, err);
            } else {
                generate_set(v[1], db, err);
            }
        } else {
            generate_all_sets(db, err);
        }
    } else
    if(t == "aliases") {
        if(sv_is_set(v[1])) {
            generate_aliases(v[1], db, err);
        } else {
            generate_all_alias_sections(db, err);
        }
    } else
    {
        ErrorList::AddError(err, ScriptVariable("unknown type: ") + t);
    }
}

void do_spooled_targets(const ScriptVariable &spooldir,
                        Database &db, ErrorList **err)
{
        /* Please note that during a single pass through the directory,
           another process could add more targets.  Hence the (a bit
           complicated) logic.
         */
    ReadDir dir(spooldir.c_str());
    bool something_done;
    for(;;) {
        something_done = false;
        const char *nm;
        while((nm = dir.Next())) {
            if(*nm == '.' || *nm == '_')
                continue;
            int res = unlink((spooldir + "/" + nm).c_str());
            if(res == -1)
                continue;
            perform_single_target(nm, db, err);
            something_done = true;
        }
        if(!something_done)
            break;
        dir.Rewind();
    }
}

static ErrorList *
perform_targets_with_spool(const ScriptVector &targets, Database &db)
{
    ErrorList *err = 0;
    ScriptVariable spooldir = db.GetSpoolDir();

    // first, we need to spool up all ``new'' targets, no locking needed
    spool_the_targets(spooldir, targets, &err);

    bool lock_ok = trylock_spooldir(spooldir);
    if(!lock_ok) {
        ErrorList::AddError(&err,
            "NOTICE: couldn't lock, targets spooled for later processing");
        return err;
    }

    do_spooled_targets(spooldir, db, &err);

    unlock_spooldir(spooldir);
    return err;
}

static ErrorList *
perform_the_targets(const ScriptVector &targets, Database &db)
{
    ErrorList *err = 0;
    int vl = targets.Length();
    int i;
    for(i = 0; i < vl; i++)
        perform_single_target(targets[i], db, &err);
    return err;
}

static ScriptVariable mk_rand_dir_name(const ScriptVariable &orig)
{
    ScriptTokenVector path(orig, "/");
    while(!sv_is_set(path[path.Length()-1]))
        path.Remove(path.Length()-1, 1);
    path[path.Length()-1] =
        ScriptVariable("_thalassa_rebuild_") +
        ScriptNumber(time(0)) + "_" + ScriptNumber(getpid());
    return path.Join("/");
}

static bool rename_to_backups(ScriptVariable base, int level)
{
    if(base.Range(-1, 1)[0] == '/')
        base.Range(-1, 1).Erase();
    ScriptVariable nmfrom =
        (level == 0) ? base : base + "." + ScriptNumber(level);
    ScriptVariable nmto = base + "." + ScriptNumber(level+1);

    FileStat fsfrom(nmfrom.c_str());
    if(!fsfrom.Exists())   // nothing to rename, good!
        return true;

    bool ok = rename_to_backups(base, level+1);
    if(!ok)
        return false;

    int res = rename(nmfrom.c_str(), nmto.c_str());
    if(res == -1)
        perror((ScriptVariable("rename: ") + nmfrom + " -> " + nmto).c_str());
    return res != -1;
}

static ErrorList* do_rebuild(Database& database, bool use_lock)
{
    ScriptVariable orig_target_dir = database.GetFilePrefix();
    ScriptVariable rand_dir = mk_rand_dir_name(orig_target_dir);
    database.SetFilePrefix(rand_dir);

        // generation into a fresh dir doesn't require locking
    ErrorList *err = generate_everything(database);

    ScriptVariableInv spooldir;
    if(use_lock) {
        spooldir = database.GetSpoolDir();
        bool lock_ok = trylock_spooldir(spooldir);
        if(!lock_ok) {
            ErrorList::AddError(&err,
                ScriptVariable("FAILURE: can't lock; tmp directory ") +
                rand_dir + " remains there, you might want to remove it");
            return err;
        }
    }
    bool ok = rename_to_backups(orig_target_dir, 0);
    if(!ok) {
        ErrorList::AddError(&err,
            ScriptVariable("ERROR: couldn't bak-rename; tmp directory ") +
            rand_dir + " remains there, you might want to remove it");
        goto quit;
    }
    int r;
    r = rename(database.GetFilePrefix().c_str(), orig_target_dir.c_str());
    if(r == -1) {
        ErrorList::AddError(&err,
            ScriptVariable("ERROR: couldn't rename the tmp dir; ") +
            rand_dir + " remains there, you might want to remove it");
    }
quit:
    if(use_lock) {
            // may look strange, but new targets could be added to the spool
            //   _after_ they were regenerated during ``generate_everything'',
            //   and it is safe to process them (even if it is ``again'')
        do_spooled_targets(spooldir, database, &err);
        unlock_spooldir(spooldir);
    }
    return err;
}

static ErrorList* generate_everything_with_spool_lock(Database& database)
{
    ErrorList *err = 0;
    ScriptVariable spooldir = database.GetSpoolDir();
    bool lock_ok = trylock_spooldir(spooldir);
    if(!lock_ok) {
        ErrorList::AddError(&err,
            "FATAL: spool dir lock failed, exiting; try again later");
        return err;
    }
    err = generate_everything(database);

        // may look strange, but new targets could be added to the spool
        //    _after_ they were regenerated during ``generate_everything'',
        //    and it is safe to process them (even if it is ``again'')
    do_spooled_targets(spooldir, database, &err);

    unlock_spooldir(spooldir);
    return err;
}



int perform_gen(cmdline_common &cmd_com, int argc, const char * const *argv)
{
    bool ok;

    GenCmdline cmdl;
    ok = parse_gen_cmdl(argc, argv, cmdl);
    if(!ok) {
        fprintf(stderr, "try ``%s help gen''\n", argv[0]);
        return 1;
    }

    Database database;
    ok = load_inifiles(database, cmd_com.inifiles, cmd_com.opt_selector);
    if(!ok)
        return 1;

    if(sv_is_set(cmdl.target_dir))
        database.SetFilePrefix(cmdl.target_dir);

    struct ErrorList *err = 0;

    if(cmdl.gen_all) {
        err = cmdl.spool ?
            generate_everything_with_spool_lock(database) :
            generate_everything(database);
    } else
    if(cmdl.rebuild) {
        err = do_rebuild(database, cmdl.spool);
    } else
    if(cmdl.targets.Length() > 0) {
        err = cmdl.spool ?
            perform_targets_with_spool(cmdl.targets, database) :
            perform_the_targets(cmdl.targets, database);
    } else {
        fprintf(stderr, "It seems I've got nothing to do.  Strange.\n");
        return 3;
    }
    if(err) {
        ErrorList *t;
        for(t = err; t; t = t->next)
            fprintf(stderr, "%s\n", t->message.c_str());
        delete err;
        return 4;
    }

    return 0;
}
