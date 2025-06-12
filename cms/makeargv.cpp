#include "makeargv.hpp"



static bool isspace(int c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool make_argv(const ScriptVariable &src, ScriptVector &target)
{
    if(src.IsInvalid())
        return false;

    target.Clear();

    enum { unquoted, singlequoted, doublequoted } state = unquoted;
    bool backslashed = false;

    ScriptVariable curword = ScriptVariableInv();

    const char *p;

    for(p = src.c_str(); *p; p++) {
        switch(state) {
        case unquoted:
            if(backslashed) {
                backslashed = false;
                break;
            }
            if(isspace(*p)) {
                if(curword.IsValid()) {
                    target.AddItem(curword);
                    curword.Invalidate();
                }
                continue;
            }
            if(*p == '\\') {
                backslashed = true;
                continue;
            }
            if(*p == '\'' || *p == '\"') {
                if(curword.IsInvalid())
                    curword = "";
                state = *p == '\'' ? singlequoted : doublequoted;
                continue;
            }
            break;
        case singlequoted:
            if(*p == '\'') {
                state = unquoted;
                continue;
            }
            break;
        case doublequoted:
            if(backslashed) {
                backslashed = false;
                break;
            }
            if(*p == '\\') {
                backslashed = true;
                continue;
            }
            if(*p == '\"') {
                state = unquoted;
                continue;
            }
            break;
        }
        if(curword.IsInvalid())
            curword = "";
        curword += *p;
    }
    if(curword.IsValid()) {
        target.AddItem(curword);
        curword.Invalidate();
    }
    return state == unquoted && !backslashed;
}



#ifdef MAKEARGV_TEST

#include <stdio.h>
#include <scriptpp/cmd.hpp>

static void print(const ScriptVector &v)
{
    int i;
    for(i = 0; i < v.Length(); i++)
        printf("[%s]\n", v[i].c_str());
}

int main(int argc, char **argv)
{
    if(argc > 1) {
        ScriptVector res;
        make_argv(argv[1], res);
        print(res);
    } else {
        ReadStream rs;
        rs.FDOpen(0);
        ScriptVariable line;
        ScriptVector res;
        while(rs.ReadLine(line)) {
            make_argv(line, res);
            print(res);
        }
    }
    return 0;
}

#endif
