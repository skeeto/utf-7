/* UTF-8 stream encoder and decoder written in ANSI C
 * This is free and unencumbered software released into the public domain.
 * Ref: https://tools.ietf.org/html/rfc3629
 */
#ifndef UTF8_H
#define UTF8_H

#include <stddef.h>

#define UTF8_OK          -1
#define UTF8_FULL        -2
#define UTF8_INCOMPLETE  -3
#define UTF8_INVALID     -4

#define UTF8_FLUSH       -1L

struct utf8 {
    char *buf;
    size_t len;
    /* internal fields */
    char hold[4];
    int n;
};

void utf8_init(struct utf8 *ctx);
int  utf8_encode(struct utf8 *ctx, long c);
long utf8_decode(struct utf8 *ctx);

#endif
