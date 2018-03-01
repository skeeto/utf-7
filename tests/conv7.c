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

struct ctx {
    char *buf;
    size_t len;
};

typedef int  (*encoder)(struct ctx *, long c);
typedef long (*decoder)(struct ctx *);

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
push(struct ctx *to, encoder en, long c, unsigned long lineno)
{
    while (en(to, c) == CTX_FULL) {
        to->buf -= BUFLEN;
        to->len = BUFLEN;
        if (!fwrite(to->buf, BUFLEN, 1, stdout))
            die("stdout:%lu: %s", lineno, strerror(errno));
    }
}

struct convert {
    struct ctx *fr;
    struct ctx *to;
    decoder de;
    encoder en;
};

static void
convert(struct convert *s, enum bom_mode bom)
{
    char bi[BUFLEN];
    char bo[BUFLEN];
    unsigned long lineno = 1;

    struct ctx *fr = s->fr;
    struct ctx *to = s->to;
    decoder de = s->de;
    encoder en = s->en;

    fr->buf = bi;
    fr->len = 0;

    to->buf = bo;
    to->len = sizeof(bo);

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
                fr->buf = bi;
                if (feof(stdin))
                    fr->len = 0;
                else
                    fr->len = fread(bi, 1, sizeof(bi), stdin);
                if (!fr->len) {
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
    if (to->buf - bo && !fwrite(bo, to->buf - bo, 1, stdout))
        die("stdout:%lu: %s", lineno, strerror(errno));
    if (fflush(stdout) == EOF)
        die("stdout:%lu: %s", lineno, strerror(errno));
}

static int
wrap_utf7_encode(struct ctx *ctx, long c)
{
    return utf7_encode((void *)ctx, c);
}

static int
wrap_utf8_encode(struct ctx *ctx, long c)
{
    return utf8_encode((void *)ctx, c);
}

static long
wrap_utf7_decode(struct ctx *ctx)
{
    return utf7_decode((void *)ctx);
}

static long
wrap_utf8_decode(struct ctx *ctx)
{
    return utf8_decode((void *)ctx);
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

    struct utf7 fr_utf7;
    struct utf8 fr_utf8;

    struct utf7 to_utf7;
    struct utf8 to_utf8;

    struct convert cvt = {0, 0, 0, 0};

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
            cvt.fr = (void *)&fr_utf7;
            cvt.de = wrap_utf7_decode;
            utf7_init(&fr_utf7, 0);
            break;
        case F_UTF8:
            cvt.fr = (void *)&fr_utf8;
            cvt.de = wrap_utf8_decode;
            utf8_init(&fr_utf8);
            break;
    }

    switch (to) {
        case F_UNKNOWN:
            abort();
            break;
        case F_UTF7:
            cvt.to = (void *)&to_utf7;
            cvt.en = wrap_utf7_encode;;
            utf7_init(&to_utf7, indirect);
            break;
        case F_UTF8:
            cvt.to = (void *)&to_utf8;
            cvt.en = wrap_utf8_encode;;
            utf8_init(&to_utf8);
            break;
    }

    convert(&cvt, bom);
    return 0;
}
