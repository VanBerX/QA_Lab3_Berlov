#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <scriptpp/scrvect.hpp>
#include <scriptpp/cmd.hpp>


    // please note these macros are also set and used in tcgi_rpl.cpp

    // the name for the file within a comment directory to store the max id
#ifndef HINT_FNAME
#define HINT_FNAME "_hints"
#endif

#ifndef MAX_COMMENT_ID
#define MAX_COMMENT_ID 50000
#endif


#if 0
    // the absolute maximum for the comment id
    // how many times we try to pick greater id in case the id
    // is already occupied, despite the information from hints and/or scan
#ifndef CID_RETRIES
#define CID_RETRIES 100
#endif

#endif



#include "database.hpp"
#include "filters.hpp"
#include "dbforum.hpp"

int CommentNode::ChildCount() const
{
    if(!children)
        return 0;
    int i;
    for(i = 0; i < children_array_size; i++)
        if(!children[i])
            return i;
    return children_array_size;  /* shouldn't happen, actually... */
}

void CommentNode::AddChild(int cid)
{
    if(!children) {
        children_array_size = 8;
        children = new int[children_array_size];
        *children = 0;
    }
    int cnt = ChildCount();
    if(cnt + 2 > children_array_size) {  /* 2 is the new child and the 0 */
        int newsize = children_array_size * 2;
        while(cnt + 2 > newsize)
            newsize *= 2;
        int *tmp = new int[newsize];
        int i;
        for(i = 0; i < cnt; i++)
            tmp[i] = children[i];
        tmp[i] = 0;
        delete[] children;
        children = tmp;
        children_array_size = newsize;
    }
    children[cnt] = cid;
    children[cnt+1] = 0;
}

CommentTree::CommentTree(const ScriptVector &auxp)
    : comments(0), comments_array_size(8), max_id(0), aux_params(auxp)
{
    comments = new CommentNode*[comments_array_size];
    *comments = new CommentNode;
    (*comments)->id = 0;
    (*comments)->parent = 0;
    int i;
    for(i = 1; i < comments_array_size; i++)
        comments[i] = 0;
}

CommentTree::~CommentTree()
{
    int i;
    for(i = 0; i <= max_id; i++)
        if(comments[i])
            delete comments[i];
    delete[] comments;
}

void CommentTree::ProvideCommentSlot(int id)
{
    if(comments_array_size > id)
        return;

    int newsize = comments_array_size * 2;
    while(newsize <= id)
        newsize *= 2;

    CommentNode **tmp = new CommentNode*[newsize];
    int i;
    for(i = 0; i <= max_id; i++)
        tmp[i] = comments[i];
    for(; i < newsize; i++)
        tmp[i] = 0;
    delete[] comments;
    comments = tmp;
    comments_array_size = newsize;
}

void CommentTree::AddComment(CommentNode *cmt)
{
    int id = cmt->id;
    if(comments_array_size > id && comments[id]) {
        // this is perfectly okay, in case one of the children was read
        // before its parent comment
        //               assert(comments[id]->id == 0)
        // we ``replant'' the children array
        if(cmt->children)  // this is a bit of paranoia, mustn't happen
            delete[] cmt->children;
        cmt->children = comments[id]->children;
        cmt->children_array_size = comments[id]->children_array_size;
        comments[id]->children = 0;      // so it won't be deleted
        delete comments[id];
    } else {
        ProvideCommentSlot(id);
    }
    comments[id] = cmt;
    if(id > max_id)
        max_id = id;
    ProvideCommentSlot(cmt->parent);
    if(!comments[cmt->parent]) {  // need temp. object to store children
        comments[cmt->parent] = new CommentNode;
        comments[cmt->parent]->id = 0;
    }
    comments[cmt->parent]->AddChild(id);
}

const CommentNode* CommentTree::GetComment(int id) const
{
    if(id > max_id)
        return 0;
    return comments[id];
}

#if 0
bool CommentTree::SetAuxData(int id, const ScriptVariable &ad)
{
    if(id < 0 || id > max_id || !comments[id])
        return false;
    comments[id]->auxdata = ad;
    return true;
}
#endif

static int read_hint(const char *hintpath)
{
    int h, r;
    FILE *f;
    f = fopen(hintpath, "r");
    if(!f)
        return -1;
    r = fscanf(f, "%d", &h);
    fclose(f);
    if(r != 1)
        return -1;
    return h;
}

static void write_hint(const char *hintpath, int val)
{
    FILE *f;
    f = fopen(hintpath, "w");
    if(!f)
        return;
    fprintf(f, "%d\n", val);
    fclose(f);
}

CommentDir::CommentDir(const ScriptVariable &pt, const ScriptVector &auxp)
    : path(pt), aux_params(auxp), tree(0), max_id(-1)
{
    int hint;
    ScriptVariable hintpath = path + "/" HINT_FNAME;
    hint = read_hint(hintpath.c_str());
    if(hint >= 0) {
        max_id = hint;
    } else {
        bool r = ScanTheTree();
        if(r) {
            max_id = tree->GetMaxId();
            write_hint(hintpath.c_str(), max_id);
        }
    }
}

CommentDir::~CommentDir()
{
    if(tree)
        delete tree;
}

CommentTree *CommentDir::GetTree()
{
    if(!tree)
        ScanTheTree();
    return tree;
}

static void
read_comment_file_to_tree(int id, const char *path, CommentTree *tree)
{
    FILE *s = fopen(path, "r");
    if(!s)
        return;

    CommentNode *node = new CommentNode;
    node->id = id;
    node->parent = -1;

    int c;
    while((c = fgetc(s)) != EOF) {
        if(!node->parser.FeedChar(c))    // ok, int
            break;
    }
    fclose(s);

    const ScriptVector& hdr = node->parser.GetHeaders();
    int i;
    for(i = 0; i < hdr.Length()-1; i+=2) {
        if(hdr[i] == "parent") {
            long n;
            if(hdr[i+1].GetLong(n, 10) && n > 0 && n <= MAX_COMMENT_ID) {
                node->parent = n;
            } else {
                // XXX error handling here?
                node->parent = 0;
            }
        } else
        if(hdr[i] == "flags") {
            ScriptVector flg(hdr[i+1], ",", " \t\r\n");
            int ln = flg.Length();
            int i;
            for(i = 0; i < ln; i++)
                if(flg[i] == "hidden") {
                    node->hidden = true;
                    break;
                }
        }
    }
    if(node->parent == -1)   // XXX not found; error?
        node->parent = 0;
    tree->AddComment(node);
}





bool CommentDir::ScanTheTree()
{
    if(tree) {
        delete tree;
        tree = 0;
    }

    ReadDir dir(path.c_str());
    if(!dir.OpenOk())
        return false;

    tree = new CommentTree(aux_params);

    const char *nm;
    while((nm = dir.Next())) {
        ScriptVariable v(nm);
        long id;
        if(!v.GetLong(id, 10))
            continue;
        read_comment_file_to_tree(id, (path+"/"+v).c_str(), tree);
    }
    return true;
}


#if 0
int CommentDir::SaveNewComment(int parent, long long date,
                               const ScriptVariable &from,
                               const ScriptVariable &title,
                               const ScriptVariable &text,
                               const ScriptVector *aux_params)
{
    if(max_id < 0)  // this probably means the directory is inaccessible
        return -1;
    int fd;
    int new_id = max_id;
    do {
        new_id++;
        if(new_id > MAX_COMMENT_ID)
            return -1;
        ScriptNumber fname(new_id);
        fd = open(fname.c_str(), O_WRONLY|O_CREAT|O_EXCL, 0666);
    } while(fd == -1 && errno == EEXIST && new_id - max_id < CID_RETRIES);
    if(fd == -1)
        return -1;

    ScriptVariable content =
        ScriptVariable(1024, "id: %d\nparent: %d\ndate: %d\n",
                       new_id, parent, date) +
        "from: " + from + "\n" +
        "title: " + title + "\n";
    if(aux_params) {
        int i;
        int len = aux_params->Length();
        for(i = 0; i < len-1; i += 2)
             content += (*aux_params)[i] + ": " + (*aux_params)[i+1] + "\n";
    }
    content += "\n";
    content += text;
    write(fd, content.c_str(), content.Length());
    close(fd);
    return new_id;
}
#endif
