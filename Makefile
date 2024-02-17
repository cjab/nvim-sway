CC = gcc
CFLAGS = -Wall -lcjson -lmsgpack-c

nvim-sway: src/main.c nvim.o sway.o
	$(CC) $(CFLAGS) -o $@ $^

nvim.o: src/nvim.c
	$(CC) $(CFLAGS) -c $^

sway.o: src/sway.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f *.o nvim-sway
