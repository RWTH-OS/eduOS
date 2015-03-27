MAKE = make
CC = gcc
CFLAGS = -O2 -Wall -m32
LDFLGAS =  
EXECFILES = $(shell find ../newlib/examples -perm -u+r+x -type f)

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
	
all: make_initrd initrd.img eduos.iso

initrd.img: $(EXECFILES) make_initrd
	@echo [MAKE_INITRD] initrd.img
	$Q./make_initrd /bin $(foreach FILE, $(EXECFILES), $(FILE) $(shell basename $(FILE)))

make_initrd: make_initrd.o
	$Q$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) 

eduos.iso: initrd.img ../eduos.elf
	@echo [MAKE_ISO] eduos.iso
	$Qcp ../eduos.elf iso/boot/
	$Qcp initrd.img iso/boot/
	$Qgenisoimage -R -b boot/grub/eltorito.img -no-emul-boot -boot-load-size 4 -boot-info-table -quiet -o eduos.iso iso

clean:
	@echo Cleaning tools
	$Q$(RM) -rf *.o *~ make_initrd initrd.img eduos.iso iso/boot/eduos.elf iso/boot/initrd.img

veryclean:
	@echo Propper cleaning tools
	$Q$(RM) -rf *.o *~ make_initrd initrd.img eduos.iso iso/boot/eduos.elf iso/boot/initrd.img

depend:
	$Q$(CC) -MM $(CFLAGS) *.c > Makefile.dep

-include Makefile.dep
# DO NOT DELETE
