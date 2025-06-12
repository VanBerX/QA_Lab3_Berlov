#ifndef FPUBLISH_HPP_SENTRY
#define FPUBLISH_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

enum file_publish_methods {
    fpm_mode_mask      = 0x0001ff,

    fpm_none           = 0x000000,
    fpm_copy           = 0x010000,
    fpm_link           = 0x020000,
    fpm_symlink        = 0x040000,

    fpm_method_mask    = 0x0f0000,

    fpm_recursive      = 0x100000,
    fpm_includehidden  = 0x200000,
    fpm_slnk_follow    = 0x400000,
    fpm_slnk_preserve  = 0x800000
};

struct ErrorList;

bool provide_dest_dir(const ScriptVariable &dir,
                      const ScriptVariable &whatfor, ErrorList **err);

bool publish_dir(const ScriptVariable &src, const ScriptVariable &dest,
                 int method, const ScriptVariable &whatfor, ErrorList **err);

bool publish_files(const ScriptVariable &srcdir, const ScriptVariable &destdir,
                   const class ScriptVector &files, int method,
                   const ScriptVariable &whatfor, ErrorList **err);

bool publish_given_file(ScriptVariable src, ScriptVariable dest,
                  int method, const ScriptVariable &whatfor, ErrorList **err);

#endif
