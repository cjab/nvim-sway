CC = gcc
CFLAGS = -Wall -lcjson -lmsgpack-c

vim-sway-nav-bin: main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f vim-sway-nav-bin
