CXXFLAGS    = -O2 -g -fno-rtti -fno-exceptions -I.. -I../../common  -I../../regex -MMD
LINKFLAGS   = -g -pthread
LIBRARIES   = ../../zlib/linux/zlib.a ../../bzlib/linux/bzlib.a ../../regex/linux/pcre.a


VPATH = ..

OBJECTS = extract.o param.o wirelist.o main.o circuit.o bdd.o multithread.o gzip.o common.o

all: extract


$(OBJECTS): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

extract : $(OBJECTS) 
	$(CXX) $(LINKFLAGS) -o $@ $(OBJECTS) $(LIBRARIES)


include *.d
