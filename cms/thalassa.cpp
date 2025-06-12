#include <stdio.h>
#include <unistd.h>

#include <scriptpp/scrvar.hpp>
#include <stfilter/stfencod.hpp>

#include "main_all.hpp"
#include "main_gen.hpp"
#include "main_lst.hpp"
#include "main_upd.hpp"
#include "_version.h"



static void help_help(FILE *stream)
{
    fprintf(stream,
        "Try\n"
        "    thalassa help <command>\n"
        "(e.g. ``thalassa help gen'')\n"
        "for command-specific help text\n"
    );
}

static void help_show(FILE *stream)
{
    fprintf(stream,
        "The ``show'' command displays properties of the program:\n\n"
        "    thalassa show <what>\n\n"
        "where <what> is one of the following:\n\n"
        "    encodings    (or just enc)   list supported encodings\n"
        "    version                      show version info\n"
    );
}

static void show_version(FILE *stream)
{
    fprintf(stream,
        "thalassa vers. " THALASSA_VERSION " (compiled " __DATE__ ")\n"
        "Copyright (c) Andrey Vikt. Stolyarov, 2023-2025\n");
}

static void help(const char *subcommand, FILE *stream)
{
    if(subcommand) {
        ScriptVariable sc(subcommand);
        if(sc == "gen")
            help_gen(stream);
        else
        if(sc == "help")
            help_help(stream);
        else
        if(sc == "show")
            help_show(stream);
        else
        if(sc == "list")
            help_list(stream);
        else
        if(sc == "update")
            help_update(stream);
        else
        if(sc == "inspect")
            help_inspect(stream);
        else
            fprintf(stderr, "unknown subcommand ``%s''\n", subcommand);
        return;
    }
    show_version(stream);
    fprintf(stream,
        "\n"
        "Usage:\n"
        "\n"
        "    thalassa [common_options] <command> [options]\n"
        "\n"
        "where <command> is one of:\n"
        "\n"
        "    help         get help\n"
        "    show         show the program's properties\n"
        "    gen          generate content\n"
        "    list         list configured/existing objects "
                                                 "(PARTIALLY IMPLEMENTED)\n"
        "    update       update a page or comment file\n"
        "    inspect      show a page or comment file's content\n"
        "\n"
        "Try    thalassa help <command> (e.g. thalassa help gen) for\n"
        "command-specific help text\n"
        "\n"
        "The following common options are recognized:\n"
        "\n"
	"    -c <dir>       change to the given directory before start\n"
        "    -o <selector>  force option selector "
                          /* sic! -> */  "(override [general]/opt_selector)\n"
        "    -i <ini_file>  load this ini file\n"
        "\n"
        "The ``-i'' option may be given multiple times; in case none\n"
        "given, the default ``thalassa.ini'' will be used.\n"
    );
}

static void print_encoding_name_list(FILE *stream)
{
    int i, n;
    n = streamfilter_encoding_name_count();
    for(i = 0; i < n; i++)
        fprintf(stream, "%s\n", streamfilter_encoding_name_by_index(i));
}

static void show(const char *subcommand, FILE *stream)
{
    if(!subcommand) {
        help_show(stream);
        return;
    }
    ScriptVariable sc(subcommand);
    if(sc == "encodings" || sc == "enc")
        print_encoding_name_list(stream);
    else
    if(sc == "version")
        show_version(stream);
    else
        help_show(stream);
}

static bool parse_common_cmdline(int argc, const char *const *argv,
                                 cmdline_common &cm, int &used_args)
{
    int c = 1;
    while(c < argc && argv[c][0] == '-') {
        if(!argv[c][1] || argv[c][2]) {
            fprintf(stderr, "option ``%s'' unrecognized\n", argv[c]);
            return false;
        }
        if(argv[c][1] == 'c' || argv[c][1] == 'i' || argv[c][1] == 'o')
        {
            if(!argv[c+1] || argv[c+1][0] == '-') {
                fprintf(stderr, "option ``%s'' requires parameter\n", argv[c]);
                return false;
            }
        }
        switch(argv[c][1]) {
        case 'c':
            if(cm.work_dir.IsValid() && cm.work_dir != "") {
                fprintf(stderr, "only one ``-c'' is allowed\n");
                return false;
            }
            cm.work_dir = argv[c+1];
            c += 2;
            break;
        case 'o':
            if(cm.opt_selector.IsValid() && cm.opt_selector != "") {
                fprintf(stderr, "only one ``-o'' is allowed\n");
                return false;
            }
            cm.opt_selector = argv[c+1];
            c += 2;
            break;
        case 'i':
            cm.inifiles.AddItem(argv[c+1]);
            c += 2;
            break;
        default:
            fprintf(stderr, "unknown option ``%s''\n", argv[c]);
            return false;
        }
    }
    // now the next parameter must be the command
    if(!argv[c]) {
        fprintf(stderr, "no command given\n");
        return false;
    }
    cm.command = argv[c];
    // in case no ini files specified, fall back to the default
    if(cm.inifiles.Length() == 0)
        cm.inifiles.AddItem("thalassa.ini");
    used_args = c;
    return true;
}

int main(int argc, const char *const *argv)
{
    if(argc < 2) {
        help(0, stderr);
        return 1;
    }
    cmdline_common cmdc;
    int used_args;
    bool ok;
    ok = parse_common_cmdline(argc, argv, cmdc, used_args);
    if(!ok) {
        help(0, stderr);
        return 1;
    }

        // please note cmdc.command is the same as argv[used_args]
        // so all the perform_XXX functions receive the command name
        // as the first (well, zeroth) element of the array

        // ``help'' and ``show'' don't need any directory change
        //     nor anything else
    if(cmdc.command == "help") {
        help(argv[used_args+1], stdout);
        return 0;
    }
    if(cmdc.command == "show") {
        show(argv[used_args+1], stdout);
        return 0;
    }

        // all the other commands depend on the current directory,
        // let's change it right now if the change is requested
    if(cmdc.work_dir.IsValid() && cmdc.work_dir != "") {
        int res = chdir(cmdc.work_dir.c_str());
        if(res == -1) {
            perror(cmdc.work_dir.c_str());
            return 2;
        }
    }

    if(cmdc.command == "gen")
        return perform_gen(cmdc, argc - used_args, argv + used_args);
    if(cmdc.command == "list")
        return perform_list(cmdc, argc - used_args, argv + used_args);
    if(cmdc.command == "update")
        return perform_update(cmdc, argc - used_args, argv + used_args);
    if(cmdc.command == "inspect")
        return perform_inspect(cmdc, argc - used_args, argv + used_args);


    fprintf(stderr, "unknown command ``%s''\n\n", argv[1]);
    help(0, stderr);
    return 1;
}
