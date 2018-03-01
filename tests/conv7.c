/* Convert between UTF-7 and other encodings on standard I/O
 * This is free and unencumbered software released into the public domain.
 */
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "utf8.h"
#include "getopt.h"
#include "../utf7.h"

#define BUFLEN 4096

#define BOM 0xfeffL

#define CTX_OK          -1
#define CTX_FULL        -2
#define CTX_INCOMPLETE  -3
#define CTX_INVALID     -4

#define CTX_FLUSH       -1L

union polyctx {
    struct {
        char *buf;
        size_t len;
    } generic;
    struct utf7 utf7;
    struct utf8 utf8;
};

typedef int  (*encoder)(union polyctx *, long c);
typedef long (*decoder)(union polyctx *);

struct ctx {
    union polyctx to;
    union polyctx fr;
    encoder encode;
    decoder decode;
};

enum encoding {
    F_UNKNOWN = 0,
    F_UTF7,
    F_UTF8
};

static void
die(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "conv7: ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
    exit(EXIT_FAILURE);
}

static struct {
    const char name[8];
    enum encoding e;
} encoding_table[] = {
    {"7", F_UTF7},
    {"utf7", F_UTF7},
    {"utf-7", F_UTF7},
    {"UTF7", F_UTF7},
    {"UTF-7", F_UTF7},
    {"8", F_UTF8},
    {"utf8", F_UTF8},
    {"utf-8", F_UTF8},
    {"UTF8", F_UTF8},
    {"UTF-8", F_UTF8}
};

static enum encoding
encoding_parse(const char *s)
{
    int i;
    for (i = 0; i < (int)sizeof(encoding_table); i++)
        if (!strcmp(s, encoding_table[i].name))
            return encoding_table[i].e;
    return F_UNKNOWN;
}

enum bom_mode {BOM_PASS, BOM_ADD, BOM_REMOVE};

static void
push(union polyctx *to, encoder encode, long c, unsigned long lineno)
{
    while (encode(to, c) == CTX_FULL) {
        to->generic.buf -= BUFLEN;
        to->generic.len = BUFLEN;
        if (!fwrite(to->generic.buf, BUFLEN, 1, stdout))
            die("stdout:%lu: %s", lineno, strerror(errno));
    }
}

static void
convert(struct ctx *ctx, enum bom_mode bom)
{
    char bi[BUFLEN];
    char bo[BUFLEN];
    unsigned long lineno = 1;

    union polyctx *fr = &ctx->fr;
    union polyctx *to = &ctx->to;
    decoder de = ctx->decode;
    encoder en = ctx->encode;

    fr->generic.buf = bi;
    fr->generic.len = 0;

    to->generic.buf = bo;
    to->generic.len = sizeof(bo);

    if (bom == BOM_ADD) {
        push(to, en, BOM, lineno);
        bom = BOM_REMOVE;
    }

    for (;;) {
        long c = de(fr);
        if (bom == BOM_REMOVE && c == BOM)
            c = de(fr);

        switch (c) {
            case CTX_OK:
            case CTX_INCOMPLETE:
                /* fetch more data */
                fr->generic.buf = bi;
                if (feof(stdin))
                    fr->generic.len = 0;
                else
                    fr->generic.len = fread(bi, 1, sizeof(bi), stdin);
                if (!fr->generic.len) {
                    if (ferror(stdin))
                        die("stdin:%lu: %s", lineno, strerror(errno));
                    if (c == UTF7_INCOMPLETE)
                        die("stdin:%lu: truncated input", lineno);
                    goto finish;
                }
                break;

            case CTX_INVALID:
                /* abort for invalid input */
                die("stdin:%lu: invalid input", lineno);
                break;

            case 0x0a:
                lineno++;
                /* FALLTHROUGH */
            default:
                push(to, en, c, lineno);
                bom = BOM_PASS;
        }
    }

finish:
    /* flush whatever is left */
    push(to, en, CTX_FLUSH, lineno);
    if (to->generic.buf - bo && !fwrite(bo, to->generic.buf - bo, 1, stdout))
        die("stdout:%lu: %s", lineno, strerror(errno));
    if (fflush(stdout) == EOF)
        die("stdout:%lu: %s", lineno, strerror(errno));
}

static int
wrap_utf7_encode(union polyctx *ctx, long c)
{
    return utf7_encode(&ctx->utf7, c);
}

static int
wrap_utf8_encode(union polyctx *ctx, long c)
{
    return utf8_encode(&ctx->utf8, c);
}

static long
wrap_utf7_decode(union polyctx *ctx)
{
    return utf7_decode(&ctx->utf7);
}

static long
wrap_utf8_decode(union polyctx *ctx)
{
    return utf8_decode(&ctx->utf8);
}

static void
usage(FILE *f)
{
    fprintf(f, "usage: conv7 -bch [-e SET] [-f FMT] [-t FMT]\n");
    fprintf(f, "  -b        add a BOM if necessary\n");
    fprintf(f, "  -c        clear a BOM if present\n");
    fprintf(f, "  -e SET    extra indirect characters (UTF-7)\n");
    fprintf(f, "  -f FMT    input encoding [utf-7]\n");
    fprintf(f, "  -h        print help info [utf-7]\n");
    fprintf(f, "  -t SET    output encoding\n");
    fprintf(f, "Supported encodings: utf-7, utf-8\n");
}

static void
set_binary_mode(void)
{
#ifdef _WIN32
    int _setmode(int, int); /* io.h */
    _setmode(_fileno(stdin), 0x8000);
    _setmode(_fileno(stdout), 0x8000);
#elif __DJGPP__
    int setmode(int, int);  /* prototypes hidden by __STRICT_ANSI__ */
    int fileno(FILE *stream);
    setmode(fileno(stdin), 0x0004);
    setmode(fileno(stdout), 0x0004);
#else /* __unix__ */
    /* nothing to do */
#endif
}

int
main(int argc, char **argv)
{
    enum bom_mode bom = BOM_PASS;
    enum encoding fr = F_UTF7;
    enum encoding to = F_UTF7;
    const char *indirect = 0;
    struct ctx ctx;

    int option;
    while ((option = getopt(argc, argv, "bce:f:ht:")) != -1) {
        switch (option) {
            case 'b':
                bom = BOM_ADD;
                break;
            case 'c':
                bom = BOM_REMOVE;
                break;
            case 'e':
                indirect = optarg;
                break;
            case 'f':
                fr = encoding_parse(optarg);
                if (!fr)
                    die("unknown encoding, %s", optarg);
                break;
            case 'h':
                usage(stdout);
                exit(EXIT_SUCCESS);
                break;
            case 't':
                to = encoding_parse(optarg);
                if (!to)
                    die("unknown encoding, %s", optarg);
                break;
            default:
                usage(stderr);
                exit(EXIT_FAILURE);
        }
    }

    if (argv[optind])
        die("unknown command line argument, %s", argv[optind]);

    /* Switch stdin/stdout to binary if necessary */
    set_binary_mode();

    switch (fr) {
        case F_UNKNOWN:
            abort();
            break;
        case F_UTF7:
            ctx.decode = wrap_utf7_decode;
            utf7_init(&ctx.fr.utf7, 0);
            break;
        case F_UTF8:
            ctx.decode = wrap_utf8_decode;
            utf8_init(&ctx.fr.utf8);
            break;
    }

    switch (to) {
        case F_UNKNOWN:
            abort();
            break;
        case F_UTF7:
            ctx.encode = wrap_utf7_encode;
            utf7_init(&ctx.to.utf7, indirect);
            break;
        case F_UTF8:
            ctx.encode = wrap_utf8_encode;
            utf8_init(&ctx.to.utf8);
            break;
    }

    convert(&ctx, bom);
    return 0;
}
