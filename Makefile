CFLAGS = -ansi -pedantic -Wall -Wextra -O3

all: tests/tests tests/utf8to7 tests/utf7to8

tests/tests: tests/tests.o utf7.o
	$(CC) $(LDFLAGS) -o $@ tests/tests.o utf7.o $(LDLIBS)

tests/utf8to7: tests/utf8to7.o utf7.o
	$(CC) $(LDFLAGS) -o $@ tests/utf8to7.o utf7.o $(LDLIBS)

tests/utf7to8: tests/utf7to8.o utf7.o
	$(CC) $(LDFLAGS) -o $@ tests/utf7to8.o utf7.o $(LDLIBS)

utf7.o: utf7.c utf7.h
tests/tests.o: tests/tests.c utf7.h
tests/utf8to7.o: tests/utf8to7.c utf7.h
tests/utf7to8.o: tests/utf7to8.c utf7.h

check: tests/tests
	tests/tests

clean:
	rm -rf utf7.o tests/tests.o tests/utf8to7.o tests/utf7to8.o
	rm -rf tests/tests tests/utf8to7 tests/utf7to8

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
