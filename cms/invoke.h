#ifndef INVOKE_H_SENTRY
#define INVOKE_H_SENTRY

#ifdef __cplusplus
extern "C" {
#endif

/*
  returns:
      0 if everything's okay
     -1 if there's a problem with one of the syscalls
               (so it makes sense to analyse the errno ``variable'')
     -2 if the child couldn't run or finished otherwise than with exit(0)
 */
int invoke_command(char * const *argv, const char *input, int inputlen);

#ifdef __cplusplus
}
#endif

#endif
