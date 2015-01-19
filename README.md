eduOS - A teaching operating system
===================================

Introduction
------------

eduOS is a Unix-like computer operating system based on a monolithic architecture for educational purposes.
It is derived from following tutorials and software distributions.
 
0. bkerndev - Bran's Kernel Development Tutorial

   The first steps to realize eduOS based on Bran's Kernel Development 
   Tutorial (http://www.osdever.net/tutorials/view/brans-kernel-development-tutorial).
   In particular, the initialization of GDT, IDT and the interrupt handlers are derived
   from this tutorial.

1. kprintf, umoddu3, udivdi3, qdivrem, divdi3, lshrdi3, moddi3, strtol, strtoul, ucmpdi2

   This software contains code derived from material licensed
   to the University of California by American Telephone and Telegraph
   Co. or Unix System Laboratories, Inc. and are reproduced herein with
   the permission of UNIX System Laboratories, Inc.

2. JamesM's kernel development tutorials
   The first version of eduOS's virtual filesystem and its initial
   ramdiks is is derived from JamesM's kernel development tutorials.
   (http://www.jamesmolloy.co.uk/tutorial_html/index.html)

Requirements of eduOS
---------------------

* Currently, eduOS supports only x86-based architectures.
* Following command line tools have to be installed:
  make, gcc, binutil, git, qemu, nams, gdb
* The test PC has to use grub as bootloader.

Building eduOS
--------------

0. Copy Makefile.example to Makefile and edit this Makefile to meet your individual convenience.
1. Copy include/eduos/config.h.example to include/eduos/config.h and edit this config file to 
   meet your individual convenience.
2. Build kernel with "make"

Start eduOS via qemu
--------------------
0. Install qemu to emulate an x86 architecture
1. Start emulator with "make qemu"

Boot eduOS via grub
-------------------
0. Copy eduos.elf as eduos.bin into the directory /boot. (cp eduos.elf /boot/eduos.bin)
1. Create a boot entry in the grub menu. This depends on the version of grub, which is used by 
   the installed Linux system. For instance, we added following lines to /boot/grub/grub.cfg:

<pre>
   ### BEGIN /etc/grub.d/40_custom ###
   # This file provides an easy way to add custom menu entries.  Simply type the
   # menu entries you want to add after this comment.  Be careful not to change
   # the 'exec tail' line above.
   menuentry "Boot eduOS!" {
          multiboot       /boot/eduos.bin
          boot
   }
</pre>

Overview of all branches
------------------------
0. stage0 - Smallest HelloWorld of the World 

   Description of loading a minimal 32bit kernel

1. stage1 - Non-preemptive multitasking

   Introduction into a simple form of multitasking, where no interrupts are
   required.

2. stage2 - Synchronisation primitives

   Description of basic synchronization primitives

3. stage3 - Preemptive multitasking

   Introduction into preemptive multitasking and interrupt handling

4. stage4 - Support of user-level tasks

   Add support of user-level tasks with an small interface for basic system calls

5. stage5 - Enabling paging

   Add support of paging. See http://www.noteblok.net/2014/06/14/bachelor
   for a detailed description.

6. stage6 - Add UART support

   Add basic support of a serial device

7. stage7 - A simple file system

   Add a virtual filesystem and a prototype of an initial ramdisk

Usefull Links
-------------
0. http://www.gnu.org/software/grub/manual/multiboot/
1. http://www.osdever.net/tutorials/view/brans-kernel-development-tutorial
2. http://www.jamesmolloy.co.uk/tutorial_html/index.html
3. http://techblog.lankes.org/tutorials/
4. http://www.os.rwth-aachen.de
5. http://www.noteblok.net/2014/06/14/bachelor
