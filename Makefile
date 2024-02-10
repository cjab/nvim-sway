CC = gcc
CFLAGS = -Wall -lcjson -lmsgpack-c

vim-sway-nav-bin: nvim.o sway.o src/main.c
	$(CC) $(CFLAGS) -o $@ $^

nvim.o: src/nvim.c
	$(CC) $(CFLAGS) -c $^

sway.o: src/sway.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f vim-sway-nav-bin *.o
