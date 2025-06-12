#ifndef BASESUBS_HPP_SENTRY
#define BASESUBS_HPP_SENTRY

#include <scriptpp/scrmacro.hpp>


    // NB: false by default, this property is used!
class IfCond : public ScriptMacroprocessorMacro {
    bool cond;
public:
    IfCond(const char *name)
        : ScriptMacroprocessorMacro(name), cond(false) {}
    ScriptVariable Expand(const ScriptVector &params) const;
    void SetCond(bool c) { cond = c; }
};



class BaseSubstitutions : public ScriptMacroprocessor {
public:
    BaseSubstitutions(const char *basepath);
    ~BaseSubstitutions();
};

#endif
