CFLAGS = -ansi -pedantic -Wall -Wextra -O3 -g3

all: tests/tests tests/conv7

tests/tests: tests/tests.o utf7.o
	$(CC) $(LDFLAGS) -o $@ tests/tests.o utf7.o $(LDLIBS)

conv7 = tests/conv7.o tests/utf8.o utf7.o
tests/conv7: $(conv7)
	$(CC) $(LDFLAGS) -o $@ $(conv7) $(LDLIBS)

utf7.o: utf7.c utf7.h
tests/tests.o: tests/tests.c utf7.h
tests/utf8.o: tests/utf8.c utf7.h
tests/conv7.o: tests/conv7.c utf7.h

conv7-cli.c: tests/conv7.c utf7.c tests/utf8.c utf7.h tests/utf8.h
	cat utf7.h tests/utf8.h \
	    tests/utf8.c utf7.c tests/getopt.h tests/conv7.c | \
	    sed -r 's@^(#include +".+)@/* \1 */@g' > $@

check: tests/tests
	tests/tests

amalgamation: conv7-cli.c

clean:
	rm -rf utf7.o tests/tests.o tests/tests
	rm -rf conv7-cli.c tests/conv7 $(conv7)

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
