#ifndef XCAPTCHA_HPP_SENTRY
#define XCAPTCHA_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

void set_captcha_info(const ScriptVariable &secret, int time_to_live);
ScriptVariable captcha_image_base64();
ScriptVariable captcha_ip();
ScriptVariable captcha_time();
ScriptVariable captcha_nonce();
ScriptVariable captcha_token();

enum {
    captcha_result_ok = 0,
    captcha_result_ip_mismatch = -1,
    captcha_result_expired = -2,
    captcha_result_broken_data = -3,
    captcha_result_wrong = -4
};

int captcha_validate(const ScriptVariable &form_ip,
                     const ScriptVariable &form_time,
                     const ScriptVariable &form_nonce,
                     const ScriptVariable &form_token,
                     const ScriptVariable &form_response);


#endif
