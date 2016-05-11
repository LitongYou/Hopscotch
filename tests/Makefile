CC = gcc
CFLAGS = -Wall -O3 -g
LIB =
DEPS = ../hopscotch.c ../lookup3.c
EXE = test

$(EXE): test.c
	$(CC) $(CFLAGS) -o $@ $^ $(DEPS) $(LIB)

.PHONY: clean

clean:
	rm -rf $(EXE)

