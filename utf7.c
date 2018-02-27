/* UTF-7 stream encoder and decoder written in ANSI C
 * This is free and unencumbered software released into the public domain.
 *
 * To avoid any dependence on the character set or locale used by the
 * compiler, this source file must not use any character or string
 * literals. Everything is hard-coded to ASCII, as required for UTF-7.
 */
#include "utf7.h"

#define UTF7_F_OPEN  (1U << 0)  /* a shifted encoding is open */
#define UTF7_F_USED  (1U << 1)  /* something has been encoded */

static int
utf7_isdirect(const struct utf7 *ctx, long c)
{
    return c <= 127 && ((ctx->direct[c / 16] >> (c % 16)) & 1U);
}

void
utf7_init(struct utf7 *ctx, const char *indirect)
{
    struct utf7 zero = {
        0, 0, 0, 0, 0, 0,
        {0x2600, 0x0000, 0xF7FF, 0xFFFF, 0xFFFF, 0xEFFF, 0xFFFF, 0x3FFF}
    };
    *ctx = zero;
    if (indirect) {
        for (; *indirect; indirect++) {
            int c = *indirect;
            if (c < 128)
                ctx->direct[c / 16] &= ~(1U << (c % 16));
        }
    }
}

static int
utf7_base64e(int v)
{
    static const char set[] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5a, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e,
        0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7a, 0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2b, 0x2f
    };
    return set[v];
}

static int
utf7_base64d(int v)
{
    static const signed char inv[] = {
          -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
          -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
          -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
          -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
          -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
          -1,   -1,   -1, 0x3e,   -1,   -1,   -1, 0x3f,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
        0x3c, 0x3d,   -1,   -1,   -1,   -1,   -1,   -1,
          -1, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
        0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
        0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
        0x17, 0x18, 0x19,   -1,   -1,   -1,   -1,   -1,
          -1, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
        0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
        0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
        0x31, 0x32, 0x33,   -1,   -1,   -1,   -1,   -1
    };
    return v < 128 ? inv[v] : -1;
}

/* Write out as much as possible without efficiency loss. */
static int
utf7_partial(struct utf7 *ctx)
{
    while (ctx->bits >= 6) {
        int a = (ctx->accum >> (ctx->bits - 6)) & 0x3fUL;
        if (!ctx->len)
            return UTF7_FULL;
        *ctx->buf++ = utf7_base64e(a);
        ctx->len--;
        ctx->bits -= 6;
    }
    return UTF7_OK;
}

/* Force close the current shifted encoding stream. */
static int
utf7_close(struct utf7 *ctx, int next)
{
    if (ctx->flags & UTF7_F_OPEN) {
        /* An encoding is currently open */
        if (!ctx->len)
            return UTF7_FULL;

        /* Flush remaining bits */
        if (ctx->bits) {
            int a = (ctx->accum << (6 - ctx->bits)) & 0x3fUL;
            *ctx->buf++ = utf7_base64e(a);
            ctx->len--;
            ctx->bits = 0;
        }

        /* Close the encoding */
        if (next == 0x2d || utf7_base64d(next) != -1) {
            if (!ctx->len)
                return UTF7_FULL;
            *ctx->buf++ = 0x2d; /* '-' */
            ctx->len--;
        }
        ctx->flags &= ~UTF7_F_OPEN;
    }
    return UTF7_OK;
}

int
utf7_encode(struct utf7 *ctx, long c)
{
    /* flush crumbs left from last code point */
    if (utf7_partial(ctx) != UTF7_OK)
        return UTF7_FULL;

    if (c == UTF7_FLUSH)
        return utf7_close(ctx, 0x2d);

    if (!utf7_isdirect(ctx, c)) {
        /* use an indirect encoding */

        /* Start encoding if not already */
        if (!(ctx->flags & UTF7_F_OPEN)) {
            if (!ctx->len)
                return UTF7_FULL;
            ctx->flags &= ~UTF7_F_USED;
            ctx->flags |= UTF7_F_OPEN;
            *ctx->buf++ = 0x2b; /* '+' */
            ctx->len--;
        }

        if (c >= 0x10000L) {
            /* create a surrogate pair */
            unsigned long x = c - 0x10000L;
            unsigned long xh = 0xd800UL + (x >> 10);
            unsigned long xl = 0xdc00UL + (x & 0x3ffUL);

            /* require at least byte to spare */
            if (!ctx->len)
                return UTF7_FULL;

            /* codepoint can now be fully consumed */
            ctx->flags |= UTF7_F_USED;
            ctx->accum <<= 16;
            ctx->accum |= xh;
            ctx->bits += 16;
            utf7_partial(ctx); /* flush any leftovers */
            ctx->accum <<= 16;
            ctx->accum |= xl;
            ctx->bits += 16;
            return UTF7_OK; /* successfully consumed */

        } else if (c == 0x2b && !(ctx->flags & UTF7_F_USED)) {
            /* '+' special case */
            if (!ctx->len)
                return UTF7_FULL;
            *ctx->buf++ = 0x2d; /* '-' */
            ctx->len--;
            ctx->flags &= ~UTF7_F_OPEN;
            return UTF7_OK; /* successfully consumed */

        } else {
            /* plain old encoding */
            ctx->accum <<= 16;
            ctx->accum |= c;
            ctx->bits += 16;
            ctx->flags |= UTF7_F_USED;
            return UTF7_OK; /* successfully consumed */
        }

    } else {
        /* use a direct encoding */

        /* close any open encodings first */
        if (utf7_close(ctx, c) != UTF7_OK)
            return UTF7_FULL;

        /* direct character write */
        if (!ctx->len)
            return UTF7_FULL;
        *ctx->buf++ = (char)c;
        ctx->len--;
        return UTF7_OK;
    }
}

static int
utf7_ishigh(long c)
{
    return c >= 0xd800L && c <= 0xdbffL;
}

static int
utf7_islow(long c)
{
    return c >= 0xdc00L && c <= 0xdfffL;
}

long
utf7_decode(struct utf7 *ctx)
{
    while (ctx->len) {
        int c = *ctx->buf++;
        ctx->len--;
        if (c < 0 || c > 127) {
            ctx->buf--;
            ctx->len++;
            return UTF7_INVALID;
        }

        if (ctx->flags & UTF7_F_OPEN) {
            /* currently shift decoding */
            int v;

            if (!(ctx->flags & UTF7_F_USED) && c == 0x2d) {
                /* "+-" encoding for '+' */
                ctx->flags &= ~UTF7_F_OPEN;
                return 0x2b;
            }

            /* continue decoding as base64 */
            v = utf7_base64d(c);
            if (v < 0) {
                /* end of encoding */
                unsigned long mask;

                if (ctx->bits >= 6) {
                    /* too many bits in accumulation buffer */
                    ctx->buf--;
                    ctx->len++;
                    return UTF7_INVALID;
                }

                mask = (1UL << ctx->bits) - 1;
                if (ctx->accum & mask) {
                    /* non-zero trailing base64 bits */
                    ctx->buf--;
                    ctx->len++;
                    return UTF7_INVALID;
                }

                ctx->flags &= ~UTF7_F_OPEN;
                /* consume closing '-' if present */
                if (c != 0x2d) {
                    if (ctx->high) {
                        /* unpaired high surrogate */
                        ctx->buf--;
                        ctx->len++;
                        return UTF7_INVALID;

                    } else if (ctx->flags & UTF7_F_USED) {
                        /* valid ending for shift encoding */
                        return c;

                    } else {
                        /* shift encoded ended without being used */
                        ctx->buf--;
                        ctx->len++;
                        return UTF7_INVALID;
                    }
                }

            } else {
                /* accumulate more base64 bits */
                ctx->flags |= UTF7_F_USED;
                ctx->accum = (ctx->accum << 6) | v;
                ctx->bits += 6;
                if (ctx->bits >= 16) {
                    /* extract a code point */
                    ctx->bits -= 16;
                    c = (ctx->accum >> ctx->bits) & 0xffff;

                    if (ctx->high) {
                        /* next code point must be low surrogate */
                        unsigned long h = ctx->high;
                        if (!utf7_islow(c)) {
                            ctx->buf--;
                            ctx->len++;
                            return UTF7_INVALID;
                        }
                        ctx->high = 0;
                        c = ((h - 0xd800UL) * 0x400UL) +
                            ((c - 0xdc00UL) + 0x10000UL);

                    } else if (utf7_ishigh(c)) {
                        /* recurse to look for low surrogate */
                        ctx->high = c;
                        return utf7_decode(ctx);
                    }

                    else if (utf7_islow(c)) {
                        /* unpaired low surrogate */
                        ctx->buf--;
                        ctx->len++;
                        return UTF7_INVALID;
                    }

                    /* not a surrogate */
                    return c;
                }
            }

        } else if (c == 0x2b) {
            /* begin decoding base64 */
            ctx->flags |= UTF7_F_OPEN;
            ctx->flags &= ~UTF7_F_USED;
            ctx->bits = 0;

        } else {
            /* direct encoded character */
            if (ctx->high) {
                /* there was an unpaired high surrogate */
                ctx->buf--;
                ctx->len++;
                return UTF7_INVALID;
            }
            return c;
        }
    }

    if (ctx->flags & UTF7_F_OPEN)
        return UTF7_INCOMPLETE;
    if (ctx->high)
        return UTF7_INCOMPLETE;
    return UTF7_OK;
}
