#ifndef MEMMAIL_HPP_SENTRY
#define MEMMAIL_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

struct EmailInfo {
    ScriptVariable status, user;
    long long date;
};

class EmailData {
    ScriptVariable dirname;
public:
    EmailData(const ScriptVariable &dirpath);
    ~EmailData() {}

    bool Get(const ScriptVariable &email, EmailInfo &info);
    bool Set(const ScriptVariable &email, const EmailInfo &info);
    bool Forget(const ScriptVariable &email);
};

#endif
