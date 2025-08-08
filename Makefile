MSGPACK_LIB = $$(pkg-config msgpack-c --libs)
CJSON_LIB = $$(pkg-config libcjson --libs)

CC = gcc
CFLAGS = -Wall $(CJSON_LIB) $(MSGPACK_LIB)

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

COMMON_DEPS = src/common.h

nvim-sway: src/main.c nvim.o sway.o
	$(CC) $(CFLAGS) -o $@ $^

nvim.o: src/nvim.c src/nvim.h $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $^

sway.o: src/sway.c src/sway.h $(COMMON_DEPS)
	$(CC) $(CFLAGS) -c $^

install:
	mkdir -p $(BINDIR)
	mkdir -p $(MANDIR)
	cp nvim-sway $(BINDIR)
	cp man/man1/nvim-sway.1 $(MANDIR)

uninstall:
	rm -f $(BINDIR)/nvim-sway
	rm -f $(MANDIR)/nvim-sway.1
	rmdir --ignore-fail-on-non-empty $(BINDIR)
	rmdir --ignore-fail-on-non-empty $(MANDIR)

clean:
	rm -f *.o **/*.gch nvim-sway
