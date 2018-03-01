#include "utf8.h"

void
utf8_init(struct utf8 *ctx)
{
    struct utf8 zero = {0, 0, {0, 0, 0, 0}, 0};
    *ctx = zero;
}

static void *
utf8_encode_1(void *buf, long c)
{
    unsigned char *s = buf;
    if (c >= (1L << 16)) {
        s[0] = 0xf0 |  (c >> 18);
        s[1] = 0x80 | ((c >> 12) & 0x3f);
        s[2] = 0x80 | ((c >>  6) & 0x3f);
        s[3] = 0x80 | ((c >>  0) & 0x3f);
        return s + 4;
    } else if (c >= (1L << 11)) {
        s[0] = 0xe0 |  (c >> 12);
        s[1] = 0x80 | ((c >>  6) & 0x3f);
        s[2] = 0x80 | ((c >>  0) & 0x3f);
        return s + 3;
    } else if (c >= (1L << 7)) {
        s[0] = 0xc0 |  (c >>  6);
        s[1] = 0x80 | ((c >>  0) & 0x3f);
        return s + 2;
    } else {
        s[0] = c;
        return s + 1;
    }
}

static void *
utf8_decode_1(void *buf, long *c)
{
    unsigned char *s = buf;
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
        next = s;
    }
    if (*c >= 0xd800 && *c <= 0xdfff)
        *c = -1; /* surrogate half */
    return next;
}

static int
utf8_partial(struct utf8 *ctx)
{
    if (ctx->n) {
        int i = 0;
        while (i < ctx->n && ctx->len) {
            *ctx->buf++ = ctx->hold[i++];
            ctx->len--;
        }
        if (i < ctx->n) {
            /* didn't finish writing buffer */
            int copied = i;
            char *p = ctx->hold;
            while (i < ctx->n)
                *p++ = ctx->hold[i++];
            ctx->n -= copied;
            return UTF8_FULL;
        }
        ctx->n = 0;
    }
    return UTF8_OK;
}

int
utf8_encode(struct utf8 *ctx, long c)
{
    if (utf8_partial(ctx) != UTF8_OK) {
        /* didn't finish flushing last code point */
        return UTF8_FULL;

    } else if (c == -1) {
        /* flush */
        return UTF8_OK;

    } else if (ctx->len < 4) {
        /* may not be enough space in output, write to temporary */
        ctx->n = (char *)utf8_encode_1(ctx->hold, c) - ctx->hold;
        utf8_partial(ctx);
        return UTF8_OK; /* successfully consumed code point */

    } else {
        /* write directly to output buffer */
        char *p = utf8_encode_1(ctx->buf, c);
        ctx->len -= p - ctx->buf;
        ctx->buf = p;
        return UTF8_OK;
    }
}

long
utf8_decode(struct utf8 *ctx)
{
    long c;          /* the character that was read */
    char *p;         /* position of next character */
    char *buf;       /* source buffer for code point */
    int len;         /* length of source buffer */
    int adjust = 0;  /* extra adjustment for updating the context */

    /* Nothing left to read */
    if (!ctx->n && !ctx->len)
        return UTF8_OK;

    if (ctx->n) {
        /* continue from previous read */
        int clen = (char *)utf8_decode_1(ctx->hold, &c) - ctx->hold;

        if (ctx->len + ctx->n < (size_t)clen) {
            /* still not enough data */
            int i;
            for (i = 0; i < (int)ctx->len; i++)
                ctx->hold[ctx->n + i] = ctx->buf[i];
            ctx->n += ctx->len;
            ctx->buf += ctx->len;
            ctx->len -= ctx->len;
            return UTF8_INCOMPLETE;

        } else {
            /* copy the rest of this character */
            int i;
            for (i = 0; i < clen - ctx->n; i++)
                ctx->hold[ctx->n + i] = ctx->buf[i];
            adjust = ctx->n;
            buf = ctx->hold;
            len = clen;
        }
    }

    else if (ctx->len < sizeof(ctx->hold)) {
        /* buffer might be incomplete */
        for (ctx->n = 0; ctx->n < (int)ctx->len; ctx->n++)
            ctx->hold[ctx->n] = ctx->buf[ctx->n];
        buf = ctx->hold;
        len = ctx->len;
    } else {
        buf = ctx->buf;
        len = 4;
    }

    /* decode from the selected buffer */
    p = utf8_decode_1(buf, &c);
    if (p - buf > len) {
        /* not enough bytes in the buffer */
        ctx->buf += ctx->len;
        ctx->len = 0;
        return UTF8_INCOMPLETE;
    }
    ctx->n = 0;
    ctx->buf += p - buf - adjust;
    ctx->len -= p - buf - adjust;

    if (c < 0)
        return UTF8_INVALID;
    return c;
}
