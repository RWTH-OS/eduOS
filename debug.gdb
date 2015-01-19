# Constant part of the script
symbol-file eduos.elf
target remote localhost:1234

set architecture i386
break main
continue
