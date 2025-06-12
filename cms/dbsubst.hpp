#ifndef DBSUBST_HPP_SENTRY
#define DBSUBST_HPP_SENTRY

#include <scriptpp/scrmacro.hpp>

#include "basesubs.hpp"

#if 0
// %n% replaced with the num given to the constructor
class NumSubstitution : public ScriptMacroprocessor {
public:
    NumSubstitution(int num);
};
#endif


#if 0
/*
    Used to make file names from templates basing on the textual id
    and the subpage index.
    %id% is replaced with the textual id
    Index of the default -1 means all the following is ignored.
    %idx% is replaced with the index like 25, but with empty string for 0
    %_idx% is replaced like _25, but with empty string for 0
    %idx0% is always replaced with the number, like 25 or 0
 */
class IdSubstitution : public ScriptMacroprocessor {
public:
    IdSubstitution(const char *id, int idx = -1);
};
#endif

/*
    %idx% is replaced with the index like 25, but with empty string for 0
    %idx_% is replaced like _25, but with empty string for 0
    %idx0% is always replaced with the number, like 25 or 0
 */
void add_index_macros(ScriptMacroprocessor &smp, int index);


  // from the database module
class Database;
struct ListData;
struct ListItemData;
struct CommentData;

  // implemented in the .cpp file, but referenced from the
  // DatabaseSubstitution class (private part)
class VarListData;
class VarListItemData;
class VarCommentData;

class DatabaseSubstitution : public BaseSubstitutions {
    VarListData *vld;
    VarListItemData *vlid;
    VarCommentData *vcmd;

    const Database *the_database;
public:
    DatabaseSubstitution(const Database *master);
    ~DatabaseSubstitution();
    void SetData(const ListData *ld, const ListItemData *lid = 0);
    void SetCmtData(const CommentData *cd);
    void ForgetData() { SetData(0, 0); SetCmtData(0); }
};

#endif
