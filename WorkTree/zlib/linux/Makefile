
CFLAGS	    = -O3 -g3
AFLAGS      = 
CXXFLAGS    = -O2 -g -fno-rtti -fno-exceptions -I.. -I../../common  -MMD
LINKFLAGS   = -g -pthread
LIBRARIES   = 


VPATH = ..

OBJECTS = adler32.o compress.o crc32.o deflate.o gzio.o infblock.o infcodes.o inffast.o inflate.o inftrees.o infutil.o trees.o uncompr.o zutil.o

all: zlib.a


$(OBJECTS): %.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

zlib.a : $(OBJECTS) 
	$(AR) $(AFLAGS) -rc $@ $(OBJECTS) 


#include *.d
