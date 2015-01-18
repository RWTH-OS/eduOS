TOPDIR = $(shell pwd)
ARCH = x86
BIT = 32

ifeq ($(ARCH),x86)
ifeq ($(BIT),32)
	TARGET=i386-eduos-elf32
endif
ifeq ($(BIT),64)
	TARGET=x86_64-eduos-elf64
endif
endif

NEWLIB = $(TOPDIR)/$(ARCH)/$(TARGET)
RM = rm -rf
CD = cd
MKDIR = mkdir
TMP = $(TOPDIR)/tmp
OPT = --disable-shared --disable-multilib --enable-newlib-hw-fp --disable-newlib-multithread --disable-newlib-reent-small

default: $(ARCH)
	$(MAKE) ARCH=$(ARCH) BIT=$(BIT) TARGET=$(TARGET) CFLAGS+="-ffreestanding -Wall -I$(NEWLIB)/include -I../../include -I../../arch/$(ARCH)/include" LDFLAGS+="-nostdlib -L$(NEWLIB)/lib" -C examples depend
	#$(MAKE) ARCH=$(ARCH) BIT=$(BIT) TARGET=$(TARGET) CFLAGS+="-ffreestanding -Wall -I$(NEWLIB)/include -I../../include -I../../arch/$(ARCH)/include" LDFLAGS+="-nostdlib -L$(NEWLIB)/lib" -C examples

$(ARCH):
	$(RM) $(TMP)
	$(MKDIR) $(TMP)
	$(CD) $(TMP); $(TOPDIR)/src/configure --target=$(TARGET) --prefix=$(TOPDIR)/$(ARCH) $(OPT) && $(MAKE) && $(MAKE) install

clean:
	$(MAKE) -C examples clean
	$(RM) $(TMP)

veryclean: clean
	$(MAKE) -C examples clean
	$(RM) $(TOPDIR)/$(ARCH)
