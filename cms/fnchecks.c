#include "fnchecks.h"

/*
    note the difference:
    failed check_username_acceptable means the name will not be
       accepted for a user to register, while
    failed check_username_safe means this username can not be
       used on the site even if the administrator creates it manually
 */

int check_username_safe(const char *uname)
{
    const char *p;
    if(!uname || !*uname)
        return 0;
    for(p = uname; *p; p++)
        if((*p < 'a' || *p > 'z') && (*p < '0' || *p > '9') && *p != '_')
            return 0;
    return 1;
}

static int check_un_len(const char *uname)
{
    int i;
    for(i = 0; i < THALASSA_CGI_MAX_USERNAME_LEN; i++)
        if(uname[i] == 0)
            return 1;
    return 0;
}

int check_username_acceptable(const char *uname)
{
    if(!uname || !*uname || !uname[1] || !check_un_len(uname))
        return 0;
    if(*uname < 'a' || *uname > 'z')
        return 0;
    return check_username_safe(uname);
}

int check_passtoken_safe(const char *passtok)
{
    const char *p;
    if(!passtok || !*passtok || !passtok[1])
        return 0;
    for(p = passtok; *p; p++)
        if((*p < 'A' || *p > 'Z') && (*p < '0' || *p > '9'))
            return 0;
    return 1;
}

static int strchr1(const char *p, int c)
{
    for(; *p; p++)
        if(*p == c)
            return 1;
    return 0;
}

int check_fname_safe(const char *uname)
{
    static const char banned[] = ",/<>=?[\\]^`";
        /* !"#$%&'()* are all before '+';  {|}~ are all after 'z'
           (see below the check inside for)
           if you think ascii isn't a fundamental world constant,
           feel free to rewrite this, but we won't accept the patch
         */
    const char *p;
    if(!uname || !*uname || *uname == '.' || *uname == '-')
        return 0;
    for(p = uname; *p; p++)
        if(*p < '+' || *p > 'z' || strchr1(banned, *p))
            return 0;
    return 1;
}
