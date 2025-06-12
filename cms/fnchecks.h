#ifndef FNCHECKS_H__SENTRY
#define FNCHECKS_H__SENTRY

#ifdef __cplusplus
extern "C" {
#endif

#ifndef THALASSA_CGI_MAX_USERNAME_LEN
#define THALASSA_CGI_MAX_USERNAME_LEN 16
#endif

int check_username_safe(const char *uname);
int check_username_acceptable(const char *uname);
int check_passtoken_safe(const char *passtok);
int check_fname_safe(const char *uname);

#ifdef __cplusplus
};
#endif

#endif
