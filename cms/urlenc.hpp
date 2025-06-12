#ifndef URLENC_HPP_SENTRY
#define URLENC_HPP_SENTRY

#include <scriptpp/scrvar.hpp>

ScriptVariable url_encode(const ScriptVariable &plain);
ScriptVariable url_decode(const ScriptVariable &encoded);

#endif
