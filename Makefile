.PHONY : all clean run new

MPIROOT = /usr

CFLAGS+= -Wall -std=c++11 -pthread -g $(INCL)
CC=gcc
# MPICC=  $(MPIROOT)/bin/mpic++
MPICC=  mpic++
INCL= -I$(MPIROOT)/include
SRCS=  block.cpp node.cpp blockchain.cpp
BIN= blockchain


all: blockchain

$(BIN): $(SRCS)
	$(MPICC) $(CFLAGS) -o $(BIN) $(SRCS)

clean:
	rm -f $(BIN) *.o

new: clean all

run: blockchain
	mpiexec --oversubscribe -np 4 ./blockchain
	#mpirun -np 4 ./blockchain