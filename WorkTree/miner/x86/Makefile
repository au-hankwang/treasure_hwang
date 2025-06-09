
CXXFLAGS    = -O0 -g -fno-rtti -fno-exceptions -I.. -I../../common  -MMD -DMARIE
LINKFLAGS   = -g -pthread
#LIBRARIES   = ../../zlib/x86/zlib.a ../../regex/x86/pcre.a
LIBRARIES   = 


VPATH = ..:../../common

OBJECTS = work.o electrical.o eval_simulator.o multithread.o

all: eval_example


$(OBJECTS): %.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $< -o $@

eval_example : $(OBJECTS) 
	$(CXX) $(LINKFLAGS) -o $@ $(OBJECTS) $(LIBRARIES)


include *.d
