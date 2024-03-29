ifeq ($(shell uname -s),Linux)
    ifeq ($(shell grep -q 'NixOS' /etc/os-release && echo $$?),0)
        DEFAULT_MSGPACK_LIB = -lmsgpack-c
    else
        DEFAULT_MSGPACK_LIB = -lmsgpackc
    endif
else
    DEFAULT_MSGPACK_LIB = -lmsgpackc
endif

MSGPACK_LIB ?= $(DEFAULT_MSGPACK_LIB)

CC = gcc
CFLAGS = -Wall -lcjson $(MSGPACK_LIB)

PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

nvim-sway: src/main.c nvim.o sway.o
	$(CC) $(CFLAGS) -o $@ $^

nvim.o: src/nvim.c
	$(CC) $(CFLAGS) -c $^

sway.o: src/sway.c
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
	rm -f *.o nvim-sway
