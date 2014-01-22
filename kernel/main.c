/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the University nor the names of its contributors
 *      may be used to endorse or promote products derived from this
 *      software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <eduos/stddef.h>
#include <eduos/stdio.h>
#include <eduos/string.h>
#include <eduos/time.h>
#include <eduos/tasks.h>
#include <eduos/processor.h>
#include <eduos/tasks.h>
#include <eduos/syscall.h>
#include <asm/irq.h>
#include <asm/irqflags.h>

/* 
 * Note that linker symbols are not variables, they have no memory allocated for
 * maintaining a value, rather their address is their value.
 */
extern const void kernel_start;
extern const void kernel_end;
extern const void bss_start;
extern const void bss_end;
extern char __BUILD_DATE;
extern char __BUILD_TIME;

static void userfoo(void* arg)
{
	char str[256];

	ksnprintf(str, 256, "hello from %s\n", (char*) arg);
	SYSCALL1(__NR_write, "hello from userfoo\n");

	//kprintf("hello from %s\n", (char*) arg);
}

static char ustack[KERNEL_STACK_SIZE];

static int wrapper(void* arg)
{
	size_t* stack = (size_t*) (ustack+KERNEL_STACK_SIZE-16);

	memset(ustack, 0xCD, KERNEL_STACK_SIZE);
	*stack-- = (size_t) arg;
	*stack = (size_t) leave_user_task; // put exit function as caller on the stack

	return jump_to_user_code((uint32_t) userfoo, (uint32_t) stack);
}

static int foo(void* arg)
{
	int i;

	for(i=0; i<10; i++) {
		kprintf("hello from %s\n", (char*) arg);
	}

	// demo of an exception
	/*i = 0;
	i = 32 / i;
	kprintf("i = %d\n", i);*/

	return 0;
}

static int eduos_init(void)
{
	// initialize .bss section
	memset((void*)&bss_start, 0x00, ((size_t) &bss_end - (size_t) &bss_start));

	system_init();
	irq_init();
	timer_init();
	koutput_init();
	multitasking_init();

	return 0;
}

int main(void)
{
	tid_t id1;
	tid_t id2;
	eduos_init();

	kprintf("This is eduOS %s Build %u, %u\n", EDUOS_VERSION, &__BUILD_DATE, &__BUILD_TIME);
	kprintf("Kernel starts at %p and ends at %p\n", &kernel_start, &kernel_end);

	irq_enable();
	system_calibration();

	kprintf("Processor frequency: %u MHz\n", get_cpu_frequency());
	
	create_kernel_task(&id1, foo, "foo1", NORMAL_PRIO);
	create_kernel_task(&id2, wrapper, "userfoo", NORMAL_PRIO);

	while(1) { 
		HALT;
	}

	return 0;
}
