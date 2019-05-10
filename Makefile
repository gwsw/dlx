OPTIM = -O3 -Wall
CFLAGS = $(OPTIM) --std=gnu99 -I../blt
CCFLAGS = $(OPTIM)
CC = gcc
CCC = g++

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^
%.o: ../blt/%.c
	$(CC) $(CFLAGS) -c -o $@ $^
%.o: %.cpp
	$(CCC) $(CCFLAGS) -c -o $@ $^

# -------------------------------------------------------------

TARGETS = tiles dlx_raw grizzly suds

all: $(TARGETS)

grizzly: grizzly.o dlx.o blt.o
	$(CC) $(CFLAGS) -o $@ $^

suds: suds.o dlx.o blt.o
	$(CC) $(CFLAGS) -o $@ $^

dlx_raw: dlx_raw.o dlx.o blt.o
	$(CC) $(CFLAGS) -o $@ $^

tiles: tiles_main.o tiles_dlx.o dlx.o tiles_pent.o tiles_help.o
	$(CCC) $(CCFLAGS) -o $@ $^

tiles_pent.c: tile/pent.tiles
	bin2c pentominos < tile/pent.tiles > $@

tiles_help.c: tiles_help.txt
	bin2c help < tiles_help.txt > $@

dlx_test: dlx_test.o dlx.o
	$(CC) $(CFLAGS) -o $@ $^

# -------------------------------------------------------------

.PHONY: all grind push clean

grind: dlx_test
	valgrind ./dlx_test

push:
	git push git@github.com:blynn/dlx.git master

clean:
	rm -f $(TARGETS) *.o tiles_pent.c tiles_help.c
