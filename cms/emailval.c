#include "emailval.h"



enum {
    start,
    localpart,
    dot_in_localpart,
    first_domtoken_start,
    first_domtoken,
    first_domtoken_dash,
    domtoken_start,
    domtoken,
    domtoken_dash,
    parse_error
};

struct ea_automaton {
    int state;
    int *errcode;
};

static void init(struct ea_automaton *a, int *errcode)
{
    a->state = start;
    a->errcode = errcode;
}

static int is_localpartchar(int c)
{
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '_' || c == '%' || c == '+' || c == '-';
}

static int is_domainpartchar(int c)
{
    return
        (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') ||
        c == '-';
}

int handle_start(struct ea_automaton *a, int c)
{
    if(c == '.') {
        *(a->errcode) = eaval_leading_dot;
        return 0;
    }
    if(c == '-') {
        *(a->errcode) = eaval_leading_dash;
        return 0;
    }
    if(is_localpartchar(c)) {
        a->state = localpart;
        return 1;
    }
    *(a->errcode) = c == '@' ? eaval_empty_local_part : eaval_illegal_char;
    return 0;
}

int handle_localpart(struct ea_automaton *a, int c)
{
    if(c == '@') {
        a->state = first_domtoken_start;
        return 1;
    }
    if(c == '.') {
        a->state = dot_in_localpart;
        return 1;
    }
    if(is_localpartchar(c)) {
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_dot_in_localpart(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_local_part_ends_with_dot;
        return 0;
    }
    if(c == '.') {
        *(a->errcode) = eaval_double_dot;
        return 0;
    }
    if(is_localpartchar(c)) {
        a->state = localpart;
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_first_domtoken_start(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_double_at;
        return 0;
    }
    if(c == '-') {
        *(a->errcode) = eaval_token_begins_with_dash;
        return 0;
    }
    if(c == '.') {
        *(a->errcode) = eaval_empty_token;
        return 0;
    }
    if(is_domainpartchar(c)) {
        a->state = first_domtoken;
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_first_domtoken(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_double_at;
        return 0;
    }
    if(c == '-') {
        a->state = first_domtoken_dash;
        return 1;
    }
    if(c == '.') {
        a->state = domtoken_start;
        return 1;
    }
    if(is_domainpartchar(c)) {
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_first_domtoken_dash(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_double_at;
        return 0;
    }
    if(c == '.') {
        *(a->errcode) = eaval_token_ends_with_dash;
        return 0;
    }
    if(is_domainpartchar(c)) {
        a->state = first_domtoken;
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_domtoken_start(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_double_at;
        return 0;
    }
    if(c == '-') {
        *(a->errcode) = eaval_token_begins_with_dash;
        return 0;
    }
    if(c == '.') {
        *(a->errcode) = eaval_empty_token;
        return 0;
    }
    if(is_domainpartchar(c)) {
        a->state = domtoken;
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_domtoken(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_double_at;
        return 0;
    }
    if(c == '-') {
        a->state = domtoken_dash;
        return 1;
    }
    if(c == '.') {
        a->state = domtoken_start;
        return 1;
    }
    if(is_domainpartchar(c)) {
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

int handle_domtoken_dash(struct ea_automaton *a, int c)
{
    if(c == '@') {
        *(a->errcode) = eaval_double_at;
        return 0;
    }
    if(c == '.') {
        *(a->errcode) = eaval_token_ends_with_dash;
        return 0;
    }
    if(is_domainpartchar(c)) {
        a->state = domtoken;
        return 1;
    }
    *(a->errcode) = eaval_illegal_char;
    return 0;
}

static int feed_char(struct ea_automaton *a, int c)
{
    switch(a->state) {
    case start:
        return handle_start(a, c);
    case localpart:
        return handle_localpart(a, c);
    case dot_in_localpart:
        return handle_dot_in_localpart(a, c);
    case first_domtoken_start:
        return handle_first_domtoken_start(a, c);
    case first_domtoken:
        return handle_first_domtoken(a, c);
    case first_domtoken_dash:
        return handle_first_domtoken_dash(a, c);
    case domtoken_start:
        return handle_domtoken_start(a, c);
    case domtoken:
        return handle_domtoken(a, c);
    case domtoken_dash:
        return handle_domtoken_dash(a, c);
    default:
        a->state = parse_error;
        *(a->errcode) = eaval_bug;
        return 0;
    }
}

static int feed_end(struct ea_automaton *a)
{
    switch(a->state) {
    case start:
        *(a->errcode) = eaval_empty;
        return 0;
    case localpart:
    case dot_in_localpart:
        *(a->errcode) = eaval_no_domain_part;
        return 0;
    case first_domtoken_start:
        *(a->errcode) = eaval_empty_domain_part;
        return 0;
    case first_domtoken:
    case first_domtoken_dash:
        *(a->errcode) = eaval_short_domain_part;
        return 0;
    case domtoken_start:
        *(a->errcode) = eaval_trailing_dot;
        return 0;
    case domtoken:
        return 1;    /* yes, this is THE final state */
    case domtoken_dash:
        *(a->errcode) = eaval_token_ends_with_dash;
        return 0;
    case parse_error:
    default:
        *(a->errcode) = eaval_bug;
        return 0;
    }
}

int email_address_validate(const char *addr, int *errcode, int *errpos)
{
    struct ea_automaton a;
    int i, ok;

    init(&a, errcode);
    for(i = 0; addr[i]; i++) {
        ok = feed_char(&a, addr[i]);
        if(!ok) {
            *errpos = i;
            return 0;
        }
    }
    ok = feed_end(&a);
    if(!ok) {
        *errpos = i;
        return 0;
    }
    return 1;
}

const char *eaval_errcode_to_token(int errcode)
{
    switch(errcode) {
    case eaval_ok:                       return "ok";
    case eaval_empty:                    return "empty";
    case eaval_illegal_char:             return "illegal_char";
    case eaval_leading_dot:              return "leading_dot";
    case eaval_leading_dash:             return "leading_dash";
    case eaval_empty_local_part:         return "empty_local_part";
    case eaval_double_dot:               return "double_dot";
    case eaval_local_part_ends_with_dot: return "local_part_ends_with_dot";
    case eaval_no_domain_part:           return "no_domain_part";
    case eaval_empty_domain_part:        return "empty_domain_part";
    case eaval_double_at:                return "double_at";
    case eaval_empty_token:              return "empty_token";
    case eaval_short_domain_part:        return "short_domain_part";
    case eaval_trailing_dot:             return "trailing_dot";
    case eaval_token_begins_with_dash:   return "token_begins_with_dash";
    case eaval_token_ends_with_dash:     return "token_ends_with_dash";
    case eaval_bug:                      return "bug";
    }
    return "UNKNOWN";
}

#ifdef EMAILVAL_TEST

#include <stdio.h>

static const char *legal_samples[] = {
    "a@b.c",
    "alpha@beta.gamma",
    "alpha@beta.gamma.delta",
    "a-l_p%ha@beta.gamma.delta",
    "Alpha+@beta.gamma.delta",
    NULL
};

static const char *illegal_samples[] = {
    "",
    "al*pha@beta.gamma",
    "alpha@be_ta.gamma",
    "alpha@be*ta.gamma",
    ".alpha@beta.gamma",
    "-alpha@beta.gamma",
    "@beta.gamma",
    "al..pha@beta.gamma",
    "alpha.@beta.gamma",
    "alpha",
    "alpha@",
    "alpha@@beta.gamma",
    "alpha@be@ta.gamma",
    "alpha@beta.ga@mma",
    "alpha@.beta.gamma",
    "alpha@beta..gamma",
    "alpha@beta",
    "alpha@beta.gamma.",
    "alpha@-beta.gamma",
    "alpha@beta.-gamma",
    "alpha@beta-.gamma",
    "alpha@beta.gamma-.delta",
    "alpha@beta.gamma-",
    NULL
};

int main()
{
    int i, ok, errcode, errpos;
    for(i = 0; legal_samples[i]; i++) {
        ok = email_address_validate(legal_samples[i], &errcode, &errpos);
        if(!ok) {
            printf("%s validation failed (incorrectly), code %d pos %d\n",
                   legal_samples[i], errcode, errpos);
        }
    }
    for(i = 0; illegal_samples[i]; i++) {
        ok = email_address_validate(illegal_samples[i], &errcode, &errpos);
        if(ok) {
            printf("%s validation passed (incorrectly)\n", illegal_samples[i]);
        }
    }
    return 0;
}

#endif
