#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include "../utf7.h"

#if _WIN32
#  define C_RED(s)   s
#  define C_GREEN(s) s
#  define C_BOLD(s)  s
#else
#  define C_RED(s)   "\033[31;1m" s "\033[0m"
#  define C_GREEN(s) "\033[32;1m" s "\033[0m"
#  define C_BOLD(s)  "\033[1m"    s "\033[0m"
#endif

static void
wputs(const wchar_t *s)
{
    const wchar_t *p;
    for (p = s; *p; p++) {
        int z;
        char buf[16];
        z = wctomb(buf, *p);
        if (z > 0)
            fwrite(buf, z, 1, stdout);
    }
}

static int
decode_in_chunks(const wchar_t *in, char *out, size_t n, const char *indirect)
{
    int fills = 0;
    const wchar_t *p;
    struct utf7 ctx;

    utf7_init(&ctx, indirect);
    ctx.buf = out;
    ctx.len = n;

    for (p = in; *p; p++) {
        while (utf7_encode(&ctx, *p) != UTF7_OK) {
            fills++;
            ctx.len = n;
        }
    }
    while (utf7_encode(&ctx, UTF7_FLUSH) != UTF7_OK) {
        fills++;
        ctx.len = n;
    }

    return fills;
}

static int
run_chunker(const wchar_t *in, const char *expect, const char *indirect)
{
    size_t n;
    for (n = 1; ; n++) {
        int fills;
        char out[256] = {0};

        fills = decode_in_chunks(in, out, n, indirect);
        if (strcmp(out, expect)) {
            printf(C_RED("FAIL") " (n = %ld): '", (long)n);
            wputs(in);
            puts("'");
            printf("  expect: \"%s\"\n", expect);
            printf("  actual: \"%s\"\n", out);
            return 1;
        }
        if (!fills)
            break;
    }
    printf(C_GREEN("PASS") ": '");
    wputs(in);
    printf("' -> '%s'\n", expect);
    return 0;
}

static int
decode(const char *in, const wchar_t *expect, size_t size)
{
    long c;
    wchar_t out[256];
    size_t outlen = 0;
    struct utf7 ctx;

    utf7_init(&ctx, 0);
    ctx.buf = (char *)in;
    ctx.len = strlen(in);

    while ((c = utf7_decode(&ctx)) >= 0)
        out[outlen++] = c;
    out[outlen] = 0;
    if (memcmp(out, expect, size)) {
        printf(C_RED("FAIL") ": decode \"%s\"\n", in);
        printf("  expect: \"");
        wputs(expect);
        puts("\"");
        printf("  actual: \"");
        wputs(out);
        puts("\"");
        return 1;
    } else {
        printf(C_GREEN("PASS") ": decode \"%s\"\n", in);
    }
    return 0;
}

int
main(void)
{
    int fails = 0;

    setlocale(LC_ALL, "");

    {
        wchar_t in[] = L"1 + 2 = 3;";
        const char *expect = "1 +- 2 +AD0 3;";
        fails += run_chunker(in, expect, "=");
    }

    {
        wchar_t in[] = L" r^2";
        const char *expect = "+A8A-r^2";
        in[0] = 0x03c0; /* π */
        fails += run_chunker(in, expect, "=");
    }

    {
        wchar_t in[] = L"~~+";
        const char *expect = "+AH4AfgAr-";
        fails += run_chunker(in, expect, 0);
    }

    {
        wchar_t in[] = L"~-";
        const char *expect = "+AH4--";
        fails += run_chunker(in, expect, 0);
    }

    {
        wchar_t in[] = L"\\[\t]";
        const char *expect = "+AFw[+AAk]";
        fails += run_chunker(in, expect, "\t");
    }

    {
        long in = 0x1f4a9L; /* PILE OF POO */
        char out[16] = {0};
        const char *expect = "+2D3cqQ-";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = out;
        ctx.len = sizeof(out);
        utf7_encode(&ctx, in);
        utf7_encode(&ctx, UTF7_FLUSH);
        if (strcmp(out, expect)) {
            puts(C_RED("FAIL") ": PILE OF POO [U+1F4A9]");
            printf("  expect: \"%s\"\n", expect);
            printf("  actual: \"%s\"\n", out);
            fails++;
        } else {
            printf(C_GREEN("PASS") ": [U+1F4A9] -> %s\n", expect);
        }
    }

    {
        long in[2] = {0xd83dL, 0xdca9L}; /* PILE OF POO */
        char out[16] = {0};
        const char *expect = "+2D3cqQ-";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = out;
        ctx.len = sizeof(out);
        utf7_encode(&ctx, in[0]);
        utf7_encode(&ctx, in[1]);
        utf7_encode(&ctx, UTF7_FLUSH);
        if (strcmp(out, expect)) {
            puts(C_RED("FAIL") ": PILE OF POO [U+D83D, U+DCA9]");
            printf("  expect: \"%s\"\n", expect);
            printf("  actual: \"%s\"\n", out);
            fails++;
        } else {
            printf(C_GREEN("PASS") ": [U+D83D, U+DCA9] -> %s\n", expect);
        }
    }

    {
        /* GREEK SMALL LETTER PI, PILE OF POO */
        long in[2] = {0x03c0, 0x1f4a9};
        char out[16] = {0};
        const char *expect = "+A8DYPdyp-";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = out;
        ctx.len = sizeof(out);
        utf7_encode(&ctx, in[0]);
        utf7_encode(&ctx, in[1]);
        utf7_encode(&ctx, UTF7_FLUSH);
        if (strcmp(out, expect)) {
            puts(C_RED("FAIL") ": [U+03C0, U+1F4A9]");
            printf("  expect: \"%s\"\n", expect);
            printf("  actual: \"%s\"\n", out);
            fails++;
        } else {
            printf(C_GREEN("PASS") ": [U+03C0, U+1F4A9] -> %s\n", expect);
        }
    }

    {
        const char in[] = "1 +- 2 +AD0 3;";
        const wchar_t expect[] = L"1 + 2 = 3;";
        fails += decode(in, expect, sizeof(expect));
    }

    {
        const char in[] = "+A8DYPdyp-";
        const wchar_t expect[] = {0x03c0, 0x1f4a9, 0};
        fails += decode(in, expect, sizeof(expect));
    }

    {
        long r;
        const char name[] = "empty shift encode incomplete";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = "+";
        ctx.len = 1;
        r = utf7_decode(&ctx);
        if (r != UTF7_INCOMPLETE)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "empty shift encode invalid";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = "+]";
        ctx.len = 2;
        r = utf7_decode(&ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "partial shift encode invalid";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = "+A-";
        ctx.len = 3;
        r = utf7_decode(&ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r[2];
        const char name[] = "non-zero padding bits";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = "+///-";
        ctx.len = 5;
        r[0] = utf7_decode(&ctx);
        r[1] = utf7_decode(&ctx);
        if (r[0] != 0xffffL || r[1] != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r[1]);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "8-bit clean invalid";
        struct utf7 ctx;
        utf7_init(&ctx, 0);
        ctx.buf = "\xff";
        ctx.len = 1;
        r = utf7_decode(&ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "double high surrogate invalid";
        struct utf7 ctx;
        char in[] = "+2D3YPQ-";
        utf7_init(&ctx, 0);
        ctx.buf = in;
        ctx.len = sizeof(in) - 1;
        r = utf7_decode(&ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "unpaired low surrogate invalid";
        struct utf7 ctx;
        char in[] = "+3Kk-";
        utf7_init(&ctx, 0);
        ctx.buf = in;
        ctx.len = sizeof(in) - 1;
        r = utf7_decode(&ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "unpaired high surrogate invalid";
        struct utf7 ctx;
        char in[] = "+2D0]";
        utf7_init(&ctx, 0);
        ctx.buf = in;
        ctx.len = sizeof(in) - 1;
        r = utf7_decode(&ctx);
        if (r != UTF7_INVALID)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r;
        const char name[] = "unpaired high surrogate incomplete";
        struct utf7 ctx;
        char in[] = "+2D0-";
        utf7_init(&ctx, 0);
        ctx.buf = in;
        ctx.len = sizeof(in) - 1;
        r = utf7_decode(&ctx);
        if (r != UTF7_INCOMPLETE)
            printf(C_RED("FAIL") ": %s [%ld]\n", name, r);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    {
        long r[2];
        const char name[] = "decode surragate across split";
        struct utf7 ctx;
        char in[] = "+2D3cqQ-";
        utf7_init(&ctx, 0);
        ctx.buf = in;
        ctx.len = 4;
        r[0] = utf7_decode(&ctx);
        ctx.len = 4;
        r[1] = utf7_decode(&ctx);
        if (r[0] != UTF7_INCOMPLETE || r[1] != 0x1f4a9L)
            printf(C_RED("FAIL") ": %s [%04lx]\n", name, r[1]);
        else
            printf(C_GREEN("PASS") ": %s\n", name);
    }

    return fails ? EXIT_FAILURE : EXIT_SUCCESS;

    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
