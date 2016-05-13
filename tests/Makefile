CC = gcc
CFLAGS = -Wall -g -lpthread -fsanitize=address
LIB =
DEPS = ../hopscotch.c ../farmhash-c/farmhash.c
EXE = test

$(EXE): test.c
	$(CC) $(CFLAGS) -o $@ $^ $(DEPS) $(LIB)

.PHONY: clean

clean:
	rm -rf $(EXE)

