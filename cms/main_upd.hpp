#ifndef MAIN_UPD_HPP_SENTRY
#define MAIN_UPD_HPP_SENTRY

void help_update(FILE *stream);
int perform_update(struct cmdline_common &cmdc,
                   int argc, const char * const *argv);

void help_inspect(FILE *stream);
int perform_inspect(struct cmdline_common &cmdc,
                   int argc, const char * const *argv);

#endif
