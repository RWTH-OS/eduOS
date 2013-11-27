;
; Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
; All rights reserved.
;
; Redistribution and use in source and binary forms, with or without
; modification, are permitted provided that the following conditions are met:
;    * Redistributions of source code must retain the above copyright
;      notice, this list of conditions and the following disclaimer.
;    * Redistributions in binary form must reproduce the above copyright
;      notice, this list of conditions and the following disclaimer in the
;      documentation and/or other materials provided with the distribution.
;    * Neither the name of the University nor the names of its contributors
;      may be used to endorse or promote products derived from this software
;      without specific prior written permission.
;
; THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
; ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
; WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
; DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
; DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
; (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
; ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
; (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
; SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

; This is the kernel's entry point. We could either call main here,
; or we can use this to setup the stack or other nice stuff, like
; perhaps setting up the GDT and segments. Please note that interrupts
; are disabled at this point: More on interrupts later!

%include "config.inc"

[BITS 32]
; We use a special name to map this section at the begin of our kernel
; =>  Multiboot needs its magic number at the begin of the kernel
SECTION .mboot
global start
start:
    jmp stublet

; This part MUST be 4byte aligned, so we solve that issue using 'ALIGN 4'
ALIGN 4
mboot:
    ; Multiboot macros to make a few lines more readable later
    MULTIBOOT_PAGE_ALIGN	equ 1<<0
    MULTIBOOT_MEMORY_INFO	equ 1<<1
    MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
    MULTIBOOT_HEADER_FLAGS	equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO
    MULTIBOOT_CHECKSUM		equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

    ; This is the GRUB Multiboot header. A boot signature
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM

SECTION .text
ALIGN 4
stublet:
; initialize stack pointer
    mov esp, boot_stack
    add esp, KERNEL_STACK_SIZE-16
; initialize cpu features
    call cpu_init
; interpret multiboot information
    extern multiboot_init
    push ebx
    call multiboot_init
    add esp, 4

; jump to the boot processors's C code
    extern main
    call main
    jmp $

global cpu_init
cpu_init:
    mov eax, cr0
; enable caching, disable paging and fpu emulation
    and eax, 0x1ffffffb
; ...and turn on FPU exceptions
    or eax, 0x22
    mov cr0, eax
; clears the current pgd entry
    xor eax, eax
    mov cr3, eax
; at this stage, we disable the SSE support
    mov eax, cr4
    and eax, 0xfffbf9ff
    mov cr4, eax
    ret

; This will set up our new segment registers. We need to do
; something special in order to set CS. We do what is called a
; far jump. A jump that includes a segment as well as an offset.
; This is declared in C as 'extern void gdt_flush();'
global gdt_flush
extern gp
gdt_flush:
    lgdt [gp]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:flush2
flush2:
    ret

; The first 32 interrupt service routines (isr) entries correspond to exceptions. 
; Some exceptions will push an error code onto the stack which is specific to 
; the exception caused. To decrease the complexity, we handle this by pushing a
; dummy error code of 0 onto the stack for any ISR that doesn't push an error 
; code already. 
; 
; ISRs are registered as "Interrupt Gate".
; Therefore, the interrupt flag (IF) is already cleared.

; NASM macro which pushs also an pseudo error code
%macro isrstub_pseudo_error 1
    global isr%1
    isr%1:
        push byte 0 ; pseudo error code
        push byte %1
        jmp common_stub
%endmacro

; Similar to isrstub_pseudo_error, but without pushing
; a pseudo error code => The error code is already
; on the stack.
%macro isrstub 1
    global isr%1
    isr%1:
        push byte %1
        jmp common_stub
%endmacro

; create isr entries, where the number after the 
; pseudo error code represents following interrupts
; 0: Divide By Zero Exception
; 1: Debug Exception
; 2: Non Maskable Interrupt Exception
; 3: Int 3 Exception
; 4: INTO Exception
; 5: Out of Bounds Exception
; 6: Invalid Opcode Exception
; 7: Coprocessor Not Available Exception
%assign i 0 
%rep    8
    isrstub_pseudo_error i
%assign i i+1 
%endrep

; 8: Double Fault Exception (With Error Code!)
isrstub 8

; 9: Coprocessor Segment Overrun Exception
isrstub_pseudo_error 9

; 10: Bad TSS Exception (With Error Code!)
; 11: Segment Not Present Exception (With Error Code!)
; 12: Stack Fault Exception (With Error Code!)
; 13: General Protection Fault Exception (With Error Code!)
; 14: Page Fault Exception (With Error Code!)
%assign i 10
%rep 5
    isrstub i
%assign i i+1
%endrep

; 15: Reserved Exception
; 16: Floating Point Exception
; 17: Alignment Check Exception
; 18: Machine Check Exceptio
; 19-31: Reserved
%assign i 15
%rep    17
    isrstub_pseudo_error i
%assign i i+1
%endrep

; NASM macro for asynchronous interrupts (no exceptions)
%macro irqstub 1
    global irq%1
    irq%1:
        push byte 0 ; pseudo error code
        push byte 32+%1
        jmp common_stub
%endmacro

; create entries for the interrupts 0 to 23
%assign i 0
%rep    24
    irqstub i
%assign i i+1
%endrep

extern irq_handler
extern get_current_stack
extern finish_task_switch

global switch_context
ALIGN 4
switch_context:
    ; create on the stack a pseudo interrupt
    ; afterwards, we switch to the task with iret
    ; we already in kernel space => no pushing of SS required
    mov eax, [esp+4]            ; on the stack is already the address to store the old esp
    pushf                       ; push controll register
    push DWORD 0x8              ; CS
    push DWORD rollback         ; EIP
    push DWORD 0x0              ; Interrupt number
    push DWORD 0x00edbabe       ; Error code
    pusha                       ; push all general purpose registers...
    push 0x10                   ; kernel data segment
    push 0x10                   ; kernel data segment

    jmp common_switch

ALIGN 4
rollback:
    ret

ALIGN 4
common_stub:
    pusha
    push es
    push ds
    mov ax, 0x10
    mov es, ax
    mov ds, ax

    ; use the same handler for interrupts and exceptions
    push esp
    call irq_handler
    add esp, 4

    cmp eax, 0
    je no_context_switch

common_switch:
    mov [eax], esp             ; store old esp
    call get_current_stack     ; get new esp
    xchg eax, esp

    ; set task switched flag
    mov eax, cr0
    or eax, 8
    mov cr0, eax

    ; call cleanup code
    call finish_task_switch

no_context_switch:
    pop ds
    pop es
    popa
    add esp, 8
    iret

global boot_stack
ALIGN 4096
boot_stack:
TIMES (KERNEL_STACK_SIZE) DB 0xcd

SECTION .note.GNU-stack noalloc noexec nowrite progbits
