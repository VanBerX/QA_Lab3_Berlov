#ifndef DULLCGI_HPP_SENTRY
#define DULLCGI_HPP_SENTRY

//#include <scriptpp/scrvar.hpp>
//#include <scriptpp/scrvect.hpp>

#include "xcgi.hpp"

class DullCgi : public Cgi {
    class DullCgiDb *the_database;
public:
    DullCgi() : the_database(0) {}
    void SetDatabase(DullCgiDb *db) { the_database = db; }

    void AddMacro(class ScriptMacroprocessorMacro *p);
    void SetPositionals(const ScriptVector &v);
    class ScriptMacroprocessor *MakeMacroprocessorClone() const;
    static void DisposeMacroprocessorClone(ScriptMacroprocessor *p);

    ScriptVariable GetConfigValue(const ScriptVariable &sect1,
                                  const ScriptVariable &sect2,
                                  const ScriptVariable &param,
                                  const ScriptVariable &qualif,
                                  bool expand_macros) const;
    int GetSectionNames(const ScriptVariable &group_id,
                        class ScriptVector &names) const;

    void SendErrorPage(int code, const ScriptVariable &cmt);
    void SendPage(const ScriptVariable &pgid);
    void SendRedirect(const ScriptVariable &url);
};


/* **************************************************************

   Your (``user-supplied'') module is expected to implement and
   export the following 3 objects:

extern const char *dullcgi_config_path;
extern const char *dullcgi_program_name;
int dullcgi_main(DullCgi *cgi);

   Your module MUST NOT contain the ``main'' function, it is
   implemented in the dullcgi.cpp module.

   ************************************************************** */

#endif
