
CFLAGS	    = -O3 -g3
AFLAGS      = 
CXXFLAGS    = -O2 -g -fno-rtti -fno-exceptions -I.. -I../../common  -MMD
LINKFLAGS   = -g -pthread
LIBRARIES   = 


VPATH = ..

OBJECTS = bzlib.o compress.o crctable.o decompress.o huffman.o randtable.o blocksort.o

all: bzlib.a


$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

bzlib.a : $(OBJECTS) 
	$(AR) $(AFLAGS) -rc $@ $(OBJECTS) 


#include *.d
