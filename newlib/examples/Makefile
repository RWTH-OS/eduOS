ARCH = x86
BIT = 32

NEWLIB = ../x86/i386-eduos-elf32
MAKE = make
STRIP_DEBUG = --strip-debug
KEEP_DEBUG = --only-keep-debug

override LDFLAGS += -T link.ld

ifeq ($(BIT),64)
# Default section offsets in x86-64 ELF files are aligned to the page-size.
# For x86-64 the pagesize is huge (2 MB) with the consquence of large sparse
# ELF files (which lead to a huge initrd). To solve this, we manually set the page-size to 4 KB.
override LDFLAGS += -Wl,-n,-z,max-page-size=0x1000
endif

# other implicit rules
%.o : %.c
	$(CC_FOR_TARGET) -c $(CFLAGS) -o $@ $< 

default: all

all: hello

hello: hello.o
	$(CC_FOR_TARGET) $(LDFLAGS) -o $@ $<
	$(OBJCOPY_FOR_TARGET) $(KEEP_DEBUG) $@ $@.sym
	$(OBJCOPY_FOR_TARGET) $(STRIP_DEBUG) $@
	chmod a-x $@.sym

clean:
	$(RM) hello *.sym *.o *~ 

depend:
	$(CC_FOR_TARGET) -MM $(CFLAGS) *.c > Makefile.dep

-include Makefile.dep
# DO NOT DELETE
