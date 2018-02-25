/* UTF-7 stream encoder and decoder written in ANSI C
 * This is free and unencumbered software released into the public domain.
 * Ref: https://tools.ietf.org/html/rfc2152
 */
#ifndef UTF7_H
#define UTF7_H

#include <stddef.h>

/* utf7_encode() special code points */
#define UTF7_FLUSH       -1L

/* return codes */
#define UTF7_OK          -1
#define UTF7_FULL        -2
#define UTF7_INCOMPLETE  -3
#define UTF7_INVALID     -4

/* utf7_encode() flags */
#define UTF7_INDIRECT    (1U << 0)

#define UTF7_INIT(buf, len) {(buf), (len), 0, 0, 0}
struct utf7 {
    char *buf;
    size_t len;
    /* internal fields */
    unsigned long accum;
    int bits;
    unsigned flags;
};

int  utf7_encode(struct utf7 *, long codepoint, unsigned flags);
long utf7_decode(struct utf7 *);

#endif
