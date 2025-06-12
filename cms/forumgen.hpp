#ifndef FORUMGEN_HPP_SENTRY
#define FORUMGEN_HPP_SENTRY

#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>

struct ForumData {
    ScriptVariable type, top, bottom, comment_templ, tail;
    ScriptVariable no_comments_templ;
    ScriptVector custom_params;
    bool reverse;
    int per_page;
    bool hidden_hold_place;
    ScriptVariable GetCustomParam(const char *name) const;
};

class Database;


// abstract class
//
class ForumGenerator {
protected:
    const Database *the_database;
    ForumData *data;           // data and tree are owned
    class CommentTree *tree;
public:
    ForumGenerator(const Database *db, ForumData *fd, CommentTree *ct)
        : the_database(db), data(fd), tree(ct) {}
    virtual ~ForumGenerator();

        // by default, returns false
    virtual bool GetArrayInfo(int &pg_count, bool &reverse) const;

        // by default, returns zero-zero, which is useful for single-pgs
    virtual void GetIndices(int &idx_file, int &idx_array) const;
        // by default, returns pgn (only ThreadPages do something different)
    virtual int GetPageIdxValue(int pgn) const;
        // by default, does nothing
    virtual void SetMainHref(const ScriptVariable &href);
        // by default, does nothing
    virtual void SetHref(const ScriptVariable &href);
        // by default, returns empty string
    virtual ScriptVariable Build();
        // by default, returns false
    virtual bool Next();

        // MUST be called AFTER all the NextPage calls completed
    virtual ScriptVariable MakeCommentMap(); // by default, returns Inv
};


// generates one empty comment section
//
class EmptyForumGenerator : public ForumGenerator {
public:
    EmptyForumGenerator(const Database *db)
        : ForumGenerator(db, 0, 0) {}
    virtual ~EmptyForumGenerator() {}
};


// non-hierarchical comments
//
class PlainForumGenerator : public ForumGenerator {
    int pg_count;
    ScriptVector uris_of_pages;
    int cur_pg, next_cmt;
    ScriptVariable cur_href;
public:
    PlainForumGenerator(const Database *db, ForumData *fd, CommentTree *ct);
    virtual ~PlainForumGenerator() {}

    virtual bool GetArrayInfo(int &pg_count, bool &reverse) const;
    virtual void GetIndices(int &idx_file, int &idx_array) const;
    virtual void SetHref(const ScriptVariable &href);
    virtual ScriptVariable Build();
    virtual bool Next();
    virtual ScriptVariable MakeCommentMap(); // by default, returns Inv
private:
    void ScanPgCount();
    ScriptVariable BuildSingleComment(int cmt_id);
};


// single tree comments
//
class SingleTreeForumGenerator : public ForumGenerator {
public:
    SingleTreeForumGenerator(const Database *db, ForumData *fd, CommentTree *ct)
        : ForumGenerator(db, fd, ct) {}
    virtual ~SingleTreeForumGenerator() {}

    virtual ScriptVariable Build();
};


// multiple page tree comments
//      NOT ACTUALLY IMPLEMENTED!
//
class ForestForumGenerator : public ForumGenerator {
public:
    ForestForumGenerator(const Database *db, ForumData *fd, CommentTree *ct)
        : ForumGenerator(db, fd, ct) {}
    virtual ~ForestForumGenerator() {}

    virtual ScriptVariable Build();
};


// one page for top-level comments, each response thread on its own page
//
class ThreadPageForumGenerator : public ForumGenerator {
    const struct CommentNode *cnz; // comment node Zero    
    int pg_count;
    ScriptVector uris_of_threadpages, uris_for_map;
    ScriptVariable main_uri;
    int cur_pg;
    int *topicstart_ids;
    int *replies_cnt;
public:
    ThreadPageForumGenerator(const Database *, ForumData *, CommentTree *);
    virtual ~ThreadPageForumGenerator();

    virtual bool GetArrayInfo(int &pg_count, bool &reverse) const;
    virtual void GetIndices(int &idx_file, int &idx_array) const;
    virtual int GetPageIdxValue(int pgn) const;
    virtual void SetMainHref(const ScriptVariable &href);
    virtual void SetHref(const ScriptVariable &href);
    virtual ScriptVariable Build();
    virtual bool Next();
    virtual ScriptVariable MakeCommentMap(); // by default, returns Inv

private:
    ScriptVariable BuildSingleComment(int cmt_id,
                               const ScriptVariable &parent_uri,
                               int replies, const ScriptVariable &thread_uri);
};




// generates error message instead of comment section
//
class ErrorForumGenerator : public ForumGenerator {
    ScriptVariable msg;
public:
    ErrorForumGenerator(const Database *db, const ScriptVariable &m)
        : ForumGenerator(db, 0, 0), msg(m) {}
    virtual ~ErrorForumGenerator() {}

    virtual ScriptVariable Build();
};


#endif
