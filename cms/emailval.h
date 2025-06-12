#ifndef EMAILVAL_H_SENTRY
#define EMAILVAL_H_SENTRY

#ifdef __cplusplus
extern "C" {
#endif

/*
    The function email_address_validate performs a check whether a given
    string represents a valid email address.  Such a check is useful, e.g.,
    for validating user input from a webform, and the like.

    Only the address itself is validated, so things like

     John Doe <johndoe@example.com>       or just      <john@example.com>

    will be rejected.   Both '<' and '>' are considered inacceptable chars.

    This validator doesn't allow many things that are considered acceptable
    by the so-called ``standards'', but are never actually used.

    First of all, quoted local-parts are not allowed, so the following

      "this is crap"@example.com  "double..dot"@example.com
      "foo"."bar"@example.com     "john@example.net"@example.com

    and the like are rejected despite some email software might consider
    these valid.

    Second, ``comments'' within the local-part are not allowed, too, so

      (comment)johnny@example.com    johnny(comment)@example.com

    are rejected (hmmm... did you know this is allowed by the ``standards''?)

    ``Standards'' allow all of the !#$%&'*+-/=?^_`{|}~ to appear in the
    local-part, unquoted, with no limitations.  This validator, however,
    will reject !#$&'*?/^{|}~ in any position of the local-part, leaving the
    possibility to use %-+_ and, furthermore, will reject %-+ in the first
    position.

    Just like the ``standards'' specify, the dot `.' in local-part can
    appear, but not as the first nor last char, and no more than one dot
    in a row.

    As for domain-part, only FQDNs are accepted.  IP addresses in square
    brackets, like [192.168.251.1], are rejected.

    The last thing to mention is that FQDN must consist of at least two
    tokens, so, e.g.,    john@doe   is considered invalid.
 */

int email_address_validate(const char *addr, int *errcode, int *errpos);

enum {
    eaval_ok = 0,
    eaval_empty,                     /* ``'' */
    eaval_illegal_char,
    eaval_leading_dot,               /* ``.john@example.com'' */
    eaval_leading_dash,              /* ``-john@example.com'' */
    eaval_empty_local_part,          /* ``@example.com'' */
    eaval_double_dot,                /* ``john..doe@example.com'' */
    eaval_local_part_ends_with_dot,  /* ``john.@example.com'' */
    eaval_no_domain_part,            /* ``john'' */
    eaval_empty_domain_part,         /* ``john@'' */
    eaval_double_at,                 /* ``john@doe@example.com'' */
    eaval_empty_token,       /* ``john@.example.com'' ``john@example..com'' */
    eaval_short_domain_part,         /* ``john@example'' */
    eaval_trailing_dot,              /* ``john@example.com.'' */
    eaval_token_begins_with_dash,    /* ``john@a.-example.com'' */
    eaval_token_ends_with_dash,      /* ``john@a.example-.com'' */
    eaval_bug
};

const char *eaval_errcode_to_token(int errcode);

#ifdef __cplusplus
}
#endif

#endif
