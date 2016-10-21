CC = gcc
CFLAGS = -Wall -g -lpthread -fsanitize=address
LIB =
DEPS = ../hopscotch.c ../farmhash-c/farmhash.c

testapp: testapp.c
	$(CC) $(CFLAGS) -o $@ $^ $(DEPS) $(LIB)

simple: simple.c
	$(CC) $(CFLAGS) -o $@ $^ $(DEPS) $(LIB)

.PHONY: clean

clean:
	rm -rf testapp simple

