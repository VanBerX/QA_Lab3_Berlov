#ifndef MAIN_ALL_HPP_SENTRY
#define MAIN_ALL_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

struct cmdline_common {
    ScriptVector inifiles;
    ScriptVariable work_dir;
    ScriptVariableInv opt_selector;
    ScriptVariable command;
};

class Database;

bool load_inifiles(Database &database, const ScriptVector &ini_list,
                   const ScriptVariable &opt_selector);

#endif
