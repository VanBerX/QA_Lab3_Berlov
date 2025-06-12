#ifndef ERRLIST_HPP_SENTRY
#define ERRLIST_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

struct ErrorList {
    ErrorList *next;
    ErrorList *last;  /* undefined for all but the first element */
    ScriptVariable message;

    ErrorList(const ScriptVariable &msg) : next(0), last(0), message(msg) {}
    ~ErrorList();

        //! Add another item to the end of the given list
        /*! Please note that it MAY change the *lst pointer
            (e.g. this is the first item in the list) but it
            can leave it as it is as well; the pointer to the
            new item is returned, and it can differ from the *lst.
         */
    static ErrorList *AddError(ErrorList **lst, const ScriptVariable &msg);

        //! Append errors from the 'more' list to the '*lst' list
        /*! Please note al the items in 'more' become a part of
            the '*lst' and it becomes their owner.
         */
    static void AppendErrors(ErrorList **lst, ErrorList *more);
};

#endif
