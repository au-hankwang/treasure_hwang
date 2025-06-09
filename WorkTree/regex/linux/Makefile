
CCFLAGS	    = -O3 -g3
AFLAGS      = 
CXXFLAGS    = -O2 -g -fno-rtti -fno-exceptions -I.. -I../../common  -MMD
LINKFLAGS   = -g -pthread
LIBRARIES   = 


VPATH = ..

OBJECTS = chartables.o pcre_compile.o pcre_config.o pcre_dfa_exec.o pcre_exec.o pcre_fullinfo.o pcre_get.o pcre_globals.o pcre_info.o pcre_maketables.o pcre_ord2utf8.o pcre_printint.o pcre_refcount.o pcre_study.o pcre_tables.o pcre_try_flipped.o pcre_ucp_findchar.o pcre_version.o pcre_xclass.o

all: pcre.a


%.o: %.c
	$(CC) -c $(CCFLAGS) $< -o $@

pcre.a : $(OBJECTS) 
	$(AR) $(AFLAGS) -rc $@ $(OBJECTS) 


-include $(OBJECTS:.o=.d)
