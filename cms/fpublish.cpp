#include <errno.h>
#include <string.h>

#include <scriptpp/scrvect.hpp>
#include <scriptpp/cmd.hpp>

#include "fileops.hpp"
#include "errlist.hpp"

#include "fpublish.hpp"



bool provide_dest_dir(const ScriptVariable &dir,
                      const ScriptVariable &whatfor,
                      ErrorList **err)
{
    FileStat st(dir.c_str());
    if(st.Exists()) {
        if(!st.IsDir()) {
            ScriptVariable s(63, "%s is not a directory, skipping %s",
                             dir.c_str(), whatfor.c_str());
            ErrorList::AddError(err, s);
            return false;
        }
    } else {
        int res = make_directory_path(dir.c_str(), 0);
        if(res == -1) {
            ScriptVariable s(63, "couldn't mkdir %s (%s), skipping %s",
                             dir.c_str(), strerror(errno), whatfor.c_str());
            ErrorList::AddError(err, s);
            return false;
        }
    }
    return true;
}

static bool
publish_single_file(const ScriptVariable &src, const ScriptVariable &dest,
                 int method, const ScriptVariable &whatfor, ErrorList **err)
{
    FileStat st(src.c_str(), (method & fpm_slnk_follow));
    if(!st.Exists()) {  // well... dangling symlink?
        int res = -1;
        if((method & fpm_slnk_preserve))
            res = preserve_symlink(src, dest);
        if(res == -1) {
            ScriptVariable s(63, "couldn't publish %s [%s]",
                             src.c_str(), whatfor.c_str());
            ErrorList::AddError(err, s);
            return false;
        }
        return true;
    }
    if(st.IsSymlink()) {
        if((method & fpm_slnk_preserve)) {
            int res = preserve_symlink(src, dest);
            if(res == -1) {
                ScriptVariable s(63, "couldn't create symlink %s [%s]",
                                 dest.c_str(), whatfor.c_str());
                ErrorList::AddError(err, s);
                return false;
            }
            return true;
        } else {
            return true;    // successfully ignored
                // NB: this can't happen for fpm_slnk_follow so it is
                //     definitely the "ignore symlinks" mode
        }
    }
    if(st.IsDir()) {
        if((method & fpm_recursive)) {
            return publish_dir(src, dest, method, whatfor, err);
        } else
        if((method & fpm_method_mask) == fpm_symlink) {
            int res = make_symlink(src.c_str(), dest.c_str());
            if(res == -1) {
                ScriptVariable s(63, "Can't create symlink %s to %s [%s]",
                                 dest.c_str(), src.c_str(), strerror(errno));
                ErrorList::AddError(err, s);
                return false;
            }
            return true;
        } else {
            return true; // successfully ignored subdir
        }
    }
    // okay, we always ignore sockets, fifos and devices
    if(!st.IsRegularFile())
        return true;

    // now time to publish the regular file

    int res;
    switch((method & fpm_method_mask)) {
    case fpm_copy:
        res = file_copy(src.c_str(), dest.c_str(), method & fpm_mode_mask);
        break;
    case fpm_link:
        res = make_link(src.c_str(), dest.c_str());
        break;
    case fpm_symlink:
        res = make_symlink(src.c_str(), dest.c_str());
        break;
    }

    if(res == -1) {
        ScriptVariable s(63, "Can't publish %s as %s [%s]",
                         src.c_str(), dest.c_str(), strerror(errno));
        ErrorList::AddError(err, s);
        return false;
    }

    return true;
}


bool publish_dir(const ScriptVariable &src, const ScriptVariable &dest,
                 int method, const ScriptVariable &whatfor, ErrorList **err)
{
    if(!provide_dest_dir(dest, whatfor, err))
        return false;

    bool result = true;

    ReadDir dir(src.c_str());
    const char *nm;
    while((nm = dir.Next())) {
        if(*nm == '.') {
            if(!(method & fpm_includehidden))
                continue;
            if(!nm[1])                   // ``.'' entry
                continue;
            if(nm[1] == '.' && !nm[2])   // ``..'' entry
                continue;
        }
        ScriptVariable srcn = src + "/" + nm;
        ScriptVariable dstn = dest + "/" + nm;
        bool ok = publish_single_file(srcn, dstn, method, whatfor, err);
        result = result && ok;
    }
    return result;
}


bool publish_files(const ScriptVariable &srcdir, const ScriptVariable &destdir,
                   const ScriptVector &files, int method,
                   const ScriptVariable &whatfor, ErrorList **err)
{
    if((method & fpm_method_mask) == fpm_none)
        return true;

    bool result = true;
    int i;
    for(i = 0; i < files.Length(); i++) {
        ScriptVariable src = srcdir + '/' + files[i];
        ScriptVariable dst = destdir + '/' + files[i];
        bool ok = publish_single_file(src, dst, method, whatfor, err);
        result = result && ok;
    }
    return result;
}

bool publish_given_file(ScriptVariable src, ScriptVariable dst,
                  int method, const ScriptVariable &whatfor, ErrorList **err)
{
    FileStat fs(dst.c_str());
    if(fs.Exists() && fs.IsDir()) {
        ScriptVariable::Substring pos = src.Strrchr('/');
        ScriptVariable fname = pos.IsValid() ? pos.After().Get() : src;
        dst += '/';
        dst += fname;
    }
    return publish_single_file(src, dst, method, whatfor, err);
}


