#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>


#include <scriptpp/scrvar.hpp>
#include <scriptpp/scrvect.hpp>
#include <scriptpp/cmd.hpp>



#include "fileops.hpp"

static void normalize_path(ScriptVector &path)
{
    bool absolute = (path[0] == "");
    int i = 0;
    while(i < path.Length()) {
        if(path[i] == "" || path[i] == ".") {  // things like "_//_", "_/./_"
            path.Remove(i);
            continue;
        }
        if(path[i] == ".." && i>0 && path[i-1]!=".." && path[i-1]!= "") {
            path.Remove(i-1, 2);
            i--;
            continue;
        }
        i++;
    }
    if(absolute && path[0] != "")
        path.Insert(0, "");
    else
    if(!absolute && path[0] == "")
        path.Remove(0);
}

ScriptVariable short_link_path(ScriptVariable src, ScriptVariable dst)
{
    if(src[0] == '/' && dst[0] != '/') {
        ScriptVariable cwd;
        scriptpp_getcwd(cwd);
        dst = cwd + "/" + dst;
    } else
    if(src[0] != '/' && dst[0] == '/') {
        ScriptVariable cwd;
        scriptpp_getcwd(cwd);
        src = cwd + "/" + src;
    }
    // now either both are absolute, or both are relative
    ScriptVector srcv(src, "/", "");
    ScriptVector dstv(dst, "/", "");
    normalize_path(srcv);
    normalize_path(dstv);

    int minlen = srcv.Length();
    int n = dstv.Length();
    if(minlen > n)
        minlen = n;
    int i;
    for(i = 0; i < minlen; i++)
        if(srcv[i] != dstv[i])
            break;

    int steps_up = dstv.Length() - i - 1;
    if(steps_up < 0)     // this means trying to link into our own path
        return ScriptVariableInv();       // like ln -s /a/b/c/d /a/b

    if(i > 0)
        srcv.Remove(0, i);

    for(i = 0; i < steps_up; i++)
        srcv.Insert(0, "..");

    return srcv.Join("/");
}

// please note that make_directory_path is a plain C function,
//    plz don't break this so that it can be reused in other projects
int make_directory_path(const char *path, int skip_the_last)
{
    char *pc, *dr;
    int r;

    if(!skip_the_last) {
        struct stat st;
        r = stat(path, &st);
        if(r != -1) {
            if(S_ISDIR(st.st_mode)) {
                return 0;  // okay
            } else {
                errno = ENOTDIR;
                return -1;
            }
        }
        /* stat() failed... why? */
        if(errno != ENOENT)
            return -1;
    }

    /* okay, it really doesn't exist */

    pc = strdup(path);
    dr = strdup(dirname(pc));
    /* the check for "." eliminates a stupid call to stat */
    r = (strcmp(".", dr) == 0) ? 0 : make_directory_path(dr, 0);
    free(dr);
    free(pc);
    if(r == -1)
        return -1;

    r = skip_the_last ? 0 : mkdir(path, 0777);
    return r;
}

int file_copy(const char *src, const char *dst, int mode)
{
    static char buf[4096];
    int rc;
    int fs, fd;
    fs = open(src, O_RDONLY);
    if(fs == -1)
        return -1;
    fd = open(dst, O_WRONLY|O_TRUNC|O_CREAT, mode ? mode : 0666);
    if(fd == -1) {
        int e = errno;
        close(fs);
        errno = e;
        return -1;
    }
    if(mode)
        fchmod(fd, mode);

    while((rc = read(fs, buf, sizeof(buf))) > 0)
        write(fd, buf, rc);

    close(fd);
    close(fs);
    return 0;
}

int make_link(const char *src, const char *dst)
{
    int res = link(src, dst);
    if(res == -1 && errno == EEXIST) {
        unlink(dst);
        res = link(src, dst);
    }
    return res;
}


static int symlink_with_retry(const char *src, const char *dst)
{
    //printf("SYMLINK: %s  %s\n\n", src, dst);
    int res = symlink(src, dst);
    if(res == -1 && errno == EEXIST) {
        res = unlink(dst);
        if(res == -1)
            return -1;
        res = symlink(src, dst);
    }
    return res;
}

int make_symlink(const ScriptVariable &src, const ScriptVariable &dst)
{
    //printf("SRC/DST: %s   %s\n", src.c_str(), dst.c_str());
    ScriptVariable shortpath = short_link_path(src, dst);
    //printf("SHORTPATH: %s\n", shortpath.c_str());
    int r = make_directory_path(dst.c_str(), 1);
    if(r == -1)
        return -1;
    return symlink_with_retry(shortpath.c_str(), dst.c_str());
}

int preserve_symlink(const ScriptVariable &src, const ScriptVariable &dst)
{
    ScriptVariable lnk;
    scriptpp_readlink(src, lnk);
    if(lnk.IsInvalid())
        return -1;
    return symlink_with_retry(lnk.c_str(), dst.c_str());
}




#ifdef LINKPATH_TEST

#include <stdio.h>

int main(int argc, char **argv)
{
    if(argc < 3) {
        fprintf(stderr, "specify source and destination path\n");
        return 1;
    }
    ScriptVariable a(argv[1]), b(argv[2]);
    ScriptVariable c = short_link_path(a, b);
    if(c.IsValid())
        printf("%s\n", c.c_str());
    else
        fprintf(stderr, "can not link at that position!\n");
    return 0;
}

#endif
