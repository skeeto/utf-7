#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../utf7.h"

#define BUFLEN 4096

static void
die(const char *s)
{
    fprintf(stderr, "utf7to8: %s\n", s);
    exit(EXIT_FAILURE);
}

static void *
utf8_encode(void *buf, long c)
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

int
main(void)
{
    unsigned char out[8];
    unsigned char *end;
    char in[BUFLEN] = {0};

    struct utf7 ctx = UTF7_INIT(0, 0);
    ctx.buf = in;
    ctx.len = fread(in, 1, sizeof(in), stdin);
    if (ctx.len == 0 && ferror(stdin))
        die("input error");

    for (;;) {
        long c = utf7_decode(&ctx);
        switch (c) {
            case UTF7_OK:
            case UTF7_INCOMPLETE:
                ctx.buf = in;
                ctx.len = fread(in, 1, sizeof(in), stdin);
                if (!ctx.len) {
                    if (ferror(stdin))
                        die("input error");
                    if (c == UTF7_INCOMPLETE)
                        die("truncated input");
                    exit(EXIT_SUCCESS);
                }
                break;

            case UTF7_INVALID:
                die("invalid input");
                break;

            default:
                end = utf8_encode(out, c);
                if (!fwrite(out, 1, end - out, stdout) && ferror(stdout))
                    die("output error");
        }
    }
    return 0;
}
