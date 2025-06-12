#include <stdio.h>

#include "database.hpp"
#include "main_all.hpp"


static bool svec_has_elem(const ScriptVector &v, const ScriptVariable &el)
{
    int i;
    for(i = 0; i < v.Length(); i++) {
        if(v[i] == el)
            return true;
    }
    return false;
}

bool load_inifiles(Database &database, const ScriptVector &ini_list,
                   const ScriptVariable &opt_selector)
{
    if(opt_selector.IsValid())
        database.SetOptSelector("", opt_selector);

    ScriptVector to_load = ini_list;
    ScriptVector loaded;
    int i;
    for(i = 0; i < to_load.Length() /* NB: it changes! */; i++) {
        if(svec_has_elem(loaded, to_load[i]))
            continue;
        if(!database.Load(to_load[i].c_str())) {
            ScriptVariable diag;
            database.MakeErrorMessage(diag);
            fprintf(stderr, "%s:%s\n", to_load[i].c_str(), diag.c_str());
            return false;
        }
        loaded.AddItem(to_load[i]);
        ScriptVector ef;
        if(database.GetExtraFiles(ef)) {
            int k;
            for(k = 0; k < ef.Length(); k++)
                to_load.AddItem(ef[k]);
        }
    }
    return true;
}

