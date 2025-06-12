#ifndef ROLES_HPP_SENTRY
#define ROLES_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

class AccessChecker {
    ScriptVector access;
    int recent;
public:
    AccessChecker() : recent(0) {}
    ~AccessChecker() {}
    void SetRules(const ScriptVariable &access_config, int recent);

    bool Check(const ScriptVector &roles, ScriptVariable access_type) const;

    bool CanPost(const ScriptVector &roles, bool &bypass_premod) const;
    bool CanSeeHidden(const ScriptVector &roles, bool own) const;
    bool CanModerate(const ScriptVector &roles) const;
    bool CanEdit(const ScriptVector &roles, bool own, long long cmtdate) const;
};


#endif
