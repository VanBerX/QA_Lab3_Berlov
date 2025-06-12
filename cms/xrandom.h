#ifndef XRANDOM_H_SENTRY
#define XRANDOM_H_SENTRY

#ifdef __cplusplus
extern "C" {
#endif

void randomize();
void fill_random(void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
