#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utf7.h"

#define BUFLEN 4096

static void
die(const char *s)
{
    fprintf(stderr, "utf8to7: %s\n", s);
    exit(EXIT_FAILURE);
}

unsigned char *
utf8_decode(unsigned char *s, long *c)
{
    unsigned char *next;
    if (s[0] < 0x80) {
        *c = s[0];
        next = s + 1;
    } else if ((s[0] & 0xe0) == 0xc0) {
        *c = ((long)(s[0] & 0x1f) <<  6) |
             ((long)(s[1] & 0x3f) <<  0);
        next = s + 2;
    } else if ((s[0] & 0xf0) == 0xe0) {
        *c = ((long)(s[0] & 0x0f) << 12) |
             ((long)(s[1] & 0x3f) <<  6) |
             ((long)(s[2] & 0x3f) <<  0);
        next = s + 3;
    } else if ((s[0] & 0xf8) == 0xf0 && (s[0] <= 0xf4)) {
        *c = ((long)(s[0] & 0x07) << 18) |
             ((long)(s[1] & 0x3f) << 12) |
             ((long)(s[2] & 0x3f) <<  6) |
             ((long)(s[3] & 0x3f) <<  0);
        next = s + 4;
    } else {
        *c = -1; /* invalid */
        next = s + 1; /* skip this byte */
    }
    if (*c >= 0xd800 && *c <= 0xdfff)
        *c = -1; /* surrogate half */
    return next;
}

int
main(void)
{
    size_t z = 0;
    char out[BUFLEN];
    unsigned char in[BUFLEN] = {0};
    unsigned char *p = in;
    struct utf7 ctx;

    utf7_init(&ctx, 0);
    ctx.buf = out;
    ctx.len = sizeof(out) - 4;

    for (;;) {
        long c;
        if (z - (p - in) < 4) {
            size_t rem = z - (p - in);
            memmove(in, p, rem);
            p = in;
            z = rem + fread(in + rem, 1, sizeof(in) - 4 - rem, stdin);
        }
        if (p == in + z)
            break;

        p = utf8_decode(p, &c);
        if (c < 0 || p > in + z) die("invalid input");

        while (utf7_encode(&ctx, c) != UTF7_OK) {
            fwrite(out, 1, ctx.buf - out, stdout);
            ctx.buf = out;
            ctx.len = sizeof(out);
        }
    }

    while (utf7_encode(&ctx, UTF7_FLUSH) != UTF7_OK) {
        fwrite(out, 1, ctx.buf - out, stdout);
        ctx.buf = out;
        ctx.len = sizeof(out);
    }
    fwrite(out, 1, ctx.buf - out, stdout);
    return 0;
}
