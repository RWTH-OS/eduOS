MAKE = make
CC = gcc
CFLAGS = -O2 -Wall #-m32
LDFLGAS =  
FILE = ../README.md

# other implicit rules
%.o : %.c
	$(CC) -c $(CFLAGS) -o $@ $< 

default: all
	
all: make_initrd initrd.img

initrd.img: $(EXECFILES) make_initrd
	./make_initrd /tmp $(FILE) $(shell basename $(FILE))

make_initrd: make_initrd.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) 

clean:
	$(RM) -rf *.o *~ make_initrd initrd.img

depend:
	$(CC) -MM $(CFLAGS) *.c > Makefile.dep

-include Makefile.dep
# DO NOT DELETE
