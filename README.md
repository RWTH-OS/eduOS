eduOS - A learning operating system
===================================

Introduction
------------

eduOS is a Unix-like computer operating system based on a monolithic architecture for educational purposes.
It is derived from following tutorials and software distributions.
 
1. bkerndev - Bran's Kernel Development Tutorial
   The first steps to realize eduOS based on Bran's Kernel Development 
   Tutorial (http://www.osdever.net/tutorials/view/brans-kernel-development-tutorial).
   In particular, the initialization of GDT, IDT and the interrupt handlers are derived
   from this tutorial.

2. kprintf, umoddu3, udivdi3, qdivrem, divdi3, lshrdi3, moddi3, strtol, strtoul, ucmpdi2
   This software contains code derived from material licensed
   to the University of California by American Telephone and Telegraph
   Co. or Unix System Laboratories, Inc. and are reproduced herein with
   the permission of UNIX System Laboratories, Inc.


Requirements of eduOS
---------------------

* Currently, eduOS supports only x86-based architectures.
* Following command line tools have to be installed:
  make, gcc, binutil, git, qemu, nams, gdb
* The test PC has to use grub as bootloader.

Building eduOS
--------------

1. Copy Makefile.example to Makefile and edit this Makefile to meet your individual convenience.
2. Copy include/eduos/config.h.example to include/eduos/config.h and edit this config file to 
   meet your individual convenience.
3. Build kernel with "make"

Start eduOS via qemu
--------------------
1. Install qemu to emulate an x86 architecture
2. Start emulator with "make qemu"

Boot eduOS via grub
-------------------
1. Copy eduos.elf as eduos.bin into the directory /boot. (cp eduos.elf /boot/eduos.bin)
2. Create a boot entry in the grub menu. This depends on the version of grub, which is used by 
   the installed Linux system. For instance, we added following lines to /boot/grub/grub.cfg:

   \#\#\# BEGIN /etc/grub.d/40_custom \#\#\#
   \# This file provides an easy way to add custom menu entries.  Simply type the
   \# menu entries you want to add after this comment.  Be careful not to change
   \# the 'exec tail' line above.
   menuentry "Boot eduOS!" {
          multiboot       /boot/eduos.bin
          boot
   }
