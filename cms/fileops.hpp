#ifndef FILEOPS_HPP_SENTRY
#define FILEOPS_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

ScriptVariable short_link_path(ScriptVariable src, ScriptVariable dst);

int make_directory_path(const char *path, int skip_the_last);

    // mode==0 means 0666
int file_copy(const char *src, const char *dst, int mode);

int make_link(const char *src, const char *dst);

int make_symlink(const ScriptVariable &src, const ScriptVariable &dst);

int preserve_symlink(const ScriptVariable &src, const ScriptVariable &dst);

#endif
