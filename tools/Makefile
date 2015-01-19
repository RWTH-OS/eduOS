MAKE = make
CC = gcc
CFLAGS = -O2 -Wall #-m32
LDFLGAS =  
FILE = ../README.md

# Prettify output
V = 0
ifeq ($V,0)
	Q = @
	P = > /dev/null
endif

# other implicit rules
%.o : %.c
	@echo [CC] $@
	$Q$(CC) -c $(CFLAGS) -o $@ $< 

default: all
	
all: make_initrd initrd.img

initrd.img: make_initrd
	@echo [MAKE_INITRD] initrd.img
	$Q./make_initrd /tmp $(FILE) $(shell basename $(FILE))

make_initrd: make_initrd.o
	$Q$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) 

clean:
	@echo Cleaning tools
	$Q$(RM) -rf *.o *~ make_initrd initrd.img

depend:
	$Q$(CC) -MM $(CFLAGS) *.c > Makefile.dep

-include Makefile.dep
# DO NOT DELETE
