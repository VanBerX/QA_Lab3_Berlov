#ifndef CGICMSUB_HPP_SENTRY
#define CGICMSUB_HPP_SENTRY

#include "basesubs.hpp"

class Cgi;

class CommonCgiSubstitutions : public BaseSubstitutions {
public:
    CommonCgiSubstitutions(const Cgi *cgi);
    ~CommonCgiSubstitutions();
};

#endif
