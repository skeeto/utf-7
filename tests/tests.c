#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../utf7.h"

#if _WIN32
#  define C_RED(s)     s
#  define C_GREEN(s)   s
#  define C_YELLOW(s)  s
#  define C_MAGENTA(s) s
#else
#  define C_RED(s)     "\033[31;1m" s "\033[0m"
#  define C_GREEN(s)   "\033[32;1m" s "\033[0m"
#  define C_YELLOW(s)  "\033[33;1m" s "\033[0m"
#  define C_MAGENTA(s) "\033[35;1m" s "\033[0m"
#endif

static long
unicode_putc(long c)
{
    if (c > 127) {
        int r;
        if (c < 0x10000L)
            r = printf(C_YELLOW("\\u") C_MAGENTA("%04lX"), c);
        else
            r = printf(C_YELLOW("\\U") C_MAGENTA("%08lX"), c);
        return r > 0 ? c : -1;
    } else {
        return putchar(c) != EOF ? c : -1;
    }
}

static void
unicode_puts(const long *s)
{
    while (*s)
        unicode_putc(*s++);
}

/* Encode chunks of buflen bytes at a time.
 * Returns the number of times the buffer had to be "refilled."
 */
static int
encode(struct utf7 *ctx, const long *in, size_t buflen)
{
    int fills = 0;

    ctx->len = buflen;
    for (; *in; in++) {
        while (utf7_encode(ctx, *in) != UTF7_OK) {
            fills++;
            ctx->len = buflen;
        }
    }
    while (utf7_encode(ctx, UTF7_FLUSH) != UTF7_OK) {
        fills++;
        ctx->len = buflen;
    }

    return fills;
}

/* Encode a buffer using increasingly sized chunk sizes. */
static int
encode_chunker(const long *in, const char *expect, const char *indirect)
{
    int fills = -1;
    size_t n;
    for (n = 1; fills; n++) {
        char out[256] = {0};
        struct utf7 ctx[1];

        utf7_init(ctx, indirect);
        ctx->buf = out;
        fills = encode(ctx, in, n);

        if (strcmp(out, expect)) {
            printf(C_RED("FAIL") " (n = %ld): \"", (long)n);
            unicode_puts(in);
            puts("\"");
            printf("  expect: \"%s\"\n", expect);
            printf("  actual: \"%s\"\n", out);
            return 1;
        }
    }
    printf(C_GREEN("PASS") ": \"");
    unicode_puts(in);
    printf("\" -> \"%s\"\n", expect);
    return 0;
}

/* Decode chunks of buflen bytes at a time.
 * Returns the number of times the buffer had to be "refilled."
 */
static int
decode(struct utf7 *ctx, long *out, size_t buflen)
{
    int fills = 0;
    char *buf = ctx->buf;
    size_t len = strlen(ctx->buf);

    ctx->len = buflen;
    for (;;) {
        size_t remaining;
        long c = utf7_decode(ctx);
        switch (c) {
            case UTF7_OK:
            case UTF7_INCOMPLETE:
                remaining = len - (ctx->buf - buf);
                if (remaining == 0)
                    return c == UTF7_OK ? fills : -1;
                if (remaining < buflen)
                    ctx->len = remaining;
                else
                    ctx->len = buflen;
                fills++;
                break;
            case UTF7_INVALID:
                return -1;
            default:
                *out++ = c;
        }
    }
    return fills;
}

/* Decode a buffer using increasingly sized chunk sizes. */
static int
decode_chunker(const char *in, const long *expect)
{
    int fills = -1;
    size_t n;

    for (n = 1; fills; n++) {
        size_t i;
        long out[256];
        int match = 1;
        struct utf7 ctx[1];

        utf7_init(ctx, 0);
        ctx->buf = (char *)in;
        ctx->len = strlen(in);

        fills = decode(ctx, out, n);
        if (fills >= 0) {
            for (i = 0; expect[i]; i++) {
                if (expect[i] != out[i]) {
                    match = 0;
                    break;
                }
            }
        }

        if (!match || fills < 0) {
            printf(C_RED("FAIL") ": decode[%d] \"%s\"\n", (int)n, in);
            if (fills < 0) {
                printf("  invalid decode\n");
            } else {
                printf("  expect: \"");
                unicode_puts(expect);
                puts("\"");
                printf("  actual: \"");
                unicode_puts(out);
                puts("\"");
            }
            return 1;
        }
    }

    printf(C_GREEN("PASS") ": decode \"%s\"\n", in);
    return 0;
}

int
main(void)
{
    int fails = 0;

    {
        long in[] = {
            '1', ' ', '+', ' ', '2', ' ', '=', ' ', '3', ';', 0
        };
        char *expect = "1 +- 2 +AD0 3;";
        fails += encode_chunker(in, expect, "=");
    }

    {
        long in[] = {0x03c0, 'r', '^', '2', 0};
        char *expect = "+A8A-r^2";
        fails += encode_chunker(in, expect, "=");
    }

    {
        long in[] = {'~', '~', '+', 0};
        char *expect = "+AH4AfgAr-";
        fails += encode_chunker(in, expect, 0);
    }

    {
        long in[] = {'~', '-', 0};
        char *expect = "+AH4--";
        fails += encode_chunker(in, expect, 0);
    }

    {
        long in[] = {'\\', '[', '\t', ']', 0};
        char *expect = "+AFw[+AAk]";
        fails += encode_chunker(in, expect, "\t");
    }

    {
        long in[] = {0x1f4a9L, 0}; /* PILE OF POO */
        char *expect = "+2D3cqQ-";
        fails += encode_chunker(in, expect, 0);
    }

    {
        long in[] = {0xd83dL, 0xdca9L, 0}; /* PILE OF POO */
        char *expect = "+2D3cqQ-";
        fails += encode_chunker(in, expect, 0);
    }

    {
        /* GREEK SMALL LETTER PI, PILE OF POO */
        long in[] = {0x03c0, 0x1f4a9, 0};
        char *expect = "+A8DYPdyp-";
        fails += encode_chunker(in, expect, 0);
    }

    {
        char in[] = "1 +- 2 +AD0 3;";
        long expect[] = {
            '1', ' ', '+', ' ', '2', ' ', '=', ' ', '3', ';', 0
        };
        fails += decode_chunker(in, expect);
    }

    {
        /* GREEK SMALL LETTER PI, PILE OF POO */
        char in[] = "+A8DYPdyp-";
        long expect[] = {0x03c0, 0x1f4a9, 0};
        fails += decode_chunker(in, expect);
    }

    {
        long r;
        char name[] = "empty shift encode incomplete";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = "+";
        ctx->len = 1;
        r = utf7_decode(ctx);
        if (r != UTF7_INCOMPLETE)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "empty shift encode invalid";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = "+]";
        ctx->len = 2;
        r = utf7_decode(ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "partial shift encode invalid";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = "+A-";
        ctx->len = 3;
        r = utf7_decode(ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r[2];
        char name[] = "non-zero padding bits";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = "+///-";
        ctx->len = 5;
        r[0] = utf7_decode(ctx);
        r[1] = utf7_decode(ctx);
        if (r[0] != 0xffffL || r[1] != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r[1]);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "8-bit clean invalid";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = "\xff";
        ctx->len = 1;
        r = utf7_decode(ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "double high surrogate invalid";
        char in[] = "+2D3YPQ-";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = in;
        ctx->len = sizeof(in) - 1;
        r = utf7_decode(ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "unpaired low surrogate invalid";
        char in[] = "+3Kk-";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = in;
        ctx->len = sizeof(in) - 1;
        r = utf7_decode(ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "unpaired high surrogate invalid";
        char in[] = "+2D0]";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = in;
        ctx->len = sizeof(in) - 1;
        r = utf7_decode(ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        char name[] = "unpaired high surrogate incomplete";
        char in[] = "+2D0-";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = in;
        ctx->len = sizeof(in) - 1;
        r = utf7_decode(ctx);
        if (r != UTF7_INCOMPLETE)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r[2];
        char name[] = "decode surragate across split";
        char in[] = "+2D3cqQ-";
        struct utf7 ctx[1];
        utf7_init(ctx, 0);
        ctx->buf = in;
        ctx->len = 4;
        r[0] = utf7_decode(ctx);
        ctx->len = 4;
        r[1] = utf7_decode(ctx);
        if (r[0] != UTF7_INCOMPLETE || r[1] != 0x1f4a9L)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r[1]);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    return fails ? EXIT_FAILURE : EXIT_SUCCESS;

    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
