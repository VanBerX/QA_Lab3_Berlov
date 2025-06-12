#include "errlist.hpp"

ErrorList::~ErrorList()
{
    if(next)
        delete next;
}

ErrorList *ErrorList::AddError(ErrorList **lst, const ScriptVariable &msg)
{
    ErrorList *p = new ErrorList(msg);
    if(*lst) {
        (*lst)->last->next = p;
        (*lst)->last = p;
    } else {
        *lst = p;
        p->last = p;
    }
    return p;
}

void ErrorList::AppendErrors(ErrorList **lst, ErrorList *more)
{
    if(!more)
        return;
    if(*lst) {
        (*lst)->last->next = more;
        (*lst)->last = more->last;
    } else {
        *lst = more;
    }
}
