#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <scriptpp/scrvar.hpp>
#include <scriptpp/confinfo.hpp>
#include <scriptpp/cmd.hpp>

#include "emailval.h"

#include "memmail.hpp"


EmailData::EmailData(const ScriptVariable &d)
    : dirname(d)
{
}

static bool make_dir_if_necessary(const char *path)
{
    FileStat fs(path);
    if(fs.Exists() && fs.IsDir())
        return true;
    int r = mkdir(path, 0700);
    return r != -1;
}

static bool email_safe(const char *email)
{
    int errcode, errpos;   // will be ignored
    return email_address_validate(email, &errcode, &errpos);
}

static ScriptVariable mail_to_filename(const ScriptVariable &email)
{
    ScriptTokenVector v(email, "@");
    return v[1] + "__" + v[0];
}

bool EmailData::Get(const ScriptVariable &email, EmailInfo &info)
{
    if(!email_safe(email.c_str()))
        return false;
    ScriptVariable fname = dirname + "/" + mail_to_filename(email);
    ConfigInformation ci;
    if(!ci.FOpen(fname.c_str())) {
        info.status = "";
        info.user = "";
        info.date = -1;
        return true;
    }
    bool rr = ci.RunParser();
    ci.FClose();
    if(!rr)
        return false;

    info.status = ci.GetItem("status");
    info.user = ci.GetItem("user");
    ScriptVariable ds = ci.GetItem("date");
    long long n;
    bool ok = ds.GetLongLong(n, 10);
    info.date = ok ? n : -1;
    return true;
}

bool EmailData::Set(const ScriptVariable &email, const EmailInfo &info)
{
    if(!make_dir_if_necessary(dirname.c_str()))
        return false;
    if(!email_safe(email.c_str()))
        return false;
    ScriptVariable fname = dirname + "/" + mail_to_filename(email);

    FILE *f = fopen(fname.c_str(), "w");
    if(!f)
        return false;
    long long tm = time(0);
    fprintf(f, "status = %s\n" "user = %s\n" "date = %lld\n",
            info.status.c_str(), info.user.c_str(), tm);
    fclose(f);
    return true;
}

bool EmailData::Forget(const ScriptVariable &email)
{
    ScriptVariable fname = dirname + "/" + mail_to_filename(email);
    int res = unlink(fname.c_str());
    return res != -1;
}
