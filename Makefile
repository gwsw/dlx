CC = gcc
CCC = g++
OPTIM = -O3 -Wall
CFLAGS = $(OPTIM) --std=gnu99 -I../blt
CCFLAGS = $(OPTIM) --std=c++11

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $*.c
%.o: ../blt/%.c
	$(CC) $(CFLAGS) -c -o $@ ../blt/$*.c
%.o: %.cpp
	$(CCC) $(CCFLAGS) -c -o $@ $*.cpp

# -------------------------------------------------------------

TARGETS = dlx_raw grizzly suds tiles 

all: $(TARGETS)

grizzly: grizzly.o dlx.o blt.o
	$(CC) $(CFLAGS) -o $@ $^

suds: suds.o dlx.o blt.o
	$(CC) $(CFLAGS) -o $@ $^

dlx_raw: dlx_raw.o dlx.o blt.o
	$(CC) $(CFLAGS) -o $@ $^

TILES_OBJ = tiles.o tiles_pent.o tiles_help.o dlx.o
tiles.o: tiles.h
tiles: $(TILES_OBJ)
	$(CCC) $(CCFLAGS) -o $@ $(TILES_OBJ)

tiles_pent.c: tile/pent.tiles
	perl -e 'print "char pentominos[]={"; for(;;){ $$n=read(\*STDIN,$$d,1024); last if not $$n; for ($$i=0; $$i<$$n; ++$$i){ print ord(substr $$d,$$i,1), "," }} print "0};"' < $^ > $@

dlx_test: dlx_test.o dlx.o
	$(CC) $(CFLAGS) -o $@ $^

# -------------------------------------------------------------

.PHONY: all grind push clean

grind: dlx_test
	valgrind ./dlx_test

push:
	git push git@github.com:blynn/dlx.git master

clean:
	rm -f $(TARGETS) *.o tiles_pent.c 
