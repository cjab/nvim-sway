CC = gcc
CFLAGS = -Wall -lcjson -lmsgpack-c

nvim-sway: nvim.o sway.o src/main.c
	$(CC) $(CFLAGS) -o $@ $^

nvim.o: src/nvim.c
	$(CC) $(CFLAGS) -c $^

sway.o: src/sway.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f *.o nvim-sway
