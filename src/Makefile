
# This is an example Makefile that creates a static library on Linux
# You can also link libMVL directly into your program

CFLAGS=-O
CPPFLAGS=-O


libMVL.a: libMVL.o libMVL_sort.o
	ar rc $@ $+
	ranlib $@

clean:
	rm -f *.o *.a
