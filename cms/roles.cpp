#include <time.h>

#include "roles.hpp"

#if 0
class AccessChecker {
    ScriptVector access;
    ScriptVector *the_roles;
#endif

static bool split(ScriptVariable s, ScriptVariable &w1, ScriptVariable &rest)
{
    ScriptVariable::Substring ss = s.Whole();
    ScriptVariable::Substring w1ss;
    bool ok = ss.FetchWord(w1ss);
    if(!ok)
        return false;
    w1 = w1ss.Get();
    w1.Trim();
    rest = ss.Get();
    rest.Trim();
    return w1.IsValid() && w1 != "" && rest.IsValid() && rest != "";
}

#if 0
AccessChecker::AccessChecker()
{
}
#endif

void AccessChecker::SetRules(const ScriptVariable &access_config, int rcn)
{
    recent = rcn;
    access.Clear();
    ScriptVector v(access_config, ";", " \t\r\n");
    int vl = v.Length();
    int i;
    for(i = 0; i < vl; i++) {
        ScriptVariable a, r;
        bool ok = split(v[i], a, r);
        if(!ok)
            continue;
        access.AddItem(a);
        access.AddItem(r);
    }
}

static bool set_intersection(const ScriptVector &a, const ScriptVector &b)
{
    int al = a.Length();
    int bl = b.Length();
    int i, j;
    for(i = 0; i < al; i++)
        for(j = 0; j < bl; j++)
            if(a[i] == b[j])
                return true;
    return false;
}

bool AccessChecker::
Check(const ScriptVector &roles, ScriptVariable access_type) const
{
    access_type.Trim();
    int i;
    for(i = 0; i < access.Length()-1; i+=2) {
        if(access_type == access[i]) {
            ScriptVector needed(access[i+1], ",", " \t\r\n");
            return set_intersection(needed, roles);
        }
    }
    // well, this type of access is not found
    return false;
}

bool AccessChecker::CanPost(const ScriptVector &roles, bool &bypass) const
{
    if(Check(roles, "post_visible")) {
        bypass = true;
        return true;
    }
    if(Check(roles, "post")) {
        bypass = false;
        return true;
    }
    return false;
}

bool AccessChecker::CanSeeHidden(const ScriptVector &roles, bool own) const
{
    if(own && Check(roles, "see_own_hidden"))
        return true;
    if(Check(roles, "see_hidden"))
        return true;
    return false;
}

bool AccessChecker::CanModerate(const ScriptVector &roles) const
{
    return Check(roles, "moderation");
}

bool AccessChecker::
CanEdit(const ScriptVector &roles, bool own, long long cmtdate) const
{
    if(Check(roles, "edit"))
        return true;
    if(own) {
        if(Check(roles, "edit_own"))
            return true;
        if(Check(roles, "edit_own_recent")) {
            long long now = time(0);
            if(now - cmtdate < recent)
                return true;
        }
    }
    return false;
}

