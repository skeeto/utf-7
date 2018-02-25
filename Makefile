CFLAGS = -ansi -pedantic -Wall -Wextra -O3

tests/tests: tests/tests.o utf7.o
	$(CC) $(LDFLAGS) -o $@ tests/tests.o utf7.o $(LDLIBS)

utf7.o: utf7.c utf7.h
tests/tests.o: tests/tests.c utf7.h

check: tests/tests
	tests/tests

clean:
	rm -rf utf7.o tests/tests.o tests/tests

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<
