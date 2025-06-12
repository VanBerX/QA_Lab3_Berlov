#ifndef MAIN_GEN_HPP_SENTRY
#define MAIN_GEN_HPP_SENTRY

struct cmdline_common;

void help_gen(FILE *stream);
int perform_gen(cmdline_common &cmdc, int argc, const char * const *argv);

#endif
