#ifndef DBFORUM_HPP_SENTRY
#define DBFORUM_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrmsg.hpp>

/* comment IDs are from 1, the value of 0 means "no", e.g. no parent */


struct CommentNode {
    int id, parent;
    bool hidden;
    int *children;  /* zero-terminated */
    int children_array_size;
    HeadedTextMessage parser;

    CommentNode()
        : id(0), parent(0), hidden(false),
        children(0), children_array_size(0), parser(false)
        {}
    ~CommentNode() { if(children) delete[] children; }
    int ChildCount() const;
    void AddChild(int cid);
};

class CommentTree {
        /* comments[0] points to a CommentNode object which is not
           actually an existing comment, it only stores the list of
           top-level comments; this element always exists.
         */
    CommentNode **comments;
    int comments_array_size;
    int max_id;
    ScriptVector aux_params;  /* this is stored just to make it available
                                for the macroprocessor using the getter */
public:
    CommentTree(const ScriptVector &auxparm);
    ~CommentTree();

    void AddComment(CommentNode *cmt);  // ownership is transferred

    const CommentNode* GetComment(int id) const;

    int GetMaxId() const { return max_id; }
    const ScriptVector &GetAuxParams() const { return aux_params; }
private:
    void ProvideCommentSlot(int id);
    void ScanTheTree();
};

/* The directory contains files with names "0001", "0002", ..., and
   file named "_hints" which contains the biggest comment's id (as a text);
   in future, this file may contain more information, so as of now, all
   chars after the first whitespace are ignored;
   content of the _hints is considered a tip, not a rule (that is,
   the file can be absent, corrupt, or contain obsolete value).
   If there are more than 9999 comments (which is strange already),
   the files for them are named like "10000", "52034" etc., i.e., without
   padding zeroes.
*/
class CommentDir {
    ScriptVariable path;
    ScriptVector aux_params;
    CommentTree *tree;
    int max_id;
public:
    CommentDir(const ScriptVariable &path, const ScriptVector &aux);
    ~CommentDir();

        // if the tree isn't ready, it will be scanned
    CommentTree *GetTree();

    void ReleaseTree() { tree = 0; } // so it won't be deleted by destructor

        // 0 means no comments yet, -1 means error
    int GetMaxId() const { return max_id; }

private:
    bool ScanTheTree();
};


#endif
