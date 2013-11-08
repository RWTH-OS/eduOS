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

#ifndef __ARCH_PROCESSOR_H__
#define __ARCH_PROCESSOR_H__

#include <eduos/stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

inline static uint64_t rdtsc(void)
{
	uint64_t x;
	asm volatile ("rdtsc" : "=A" (x));
	return x;
}

inline static void flush_cache(void) {
	asm volatile ("wbinvd" : : : "memory");
}

inline static void invalid_cache(void) {
	asm volatile ("invd");
}

inline static int get_return_value(void) {
	int ret;

	asm volatile ("movl %%eax, %0" : "=r"(ret));

	return ret;
}

/* Force strict CPU ordering */
#ifdef CONFIG_ROCKCREEK
inline static void mb(void) { asm volatile ("lock; addl $0,0(%%esp)" ::: "memory", "cc"); }
inline static void rmb(void) { asm volatile ("lock; addl $0,0(%%esp)" ::: "memory", "cc"); }
inline static void wmb(void) { asm volatile ("lock; addl $0,0(%%esp)" ::: "memory", "cc"); }
#else
inline static void mb(void) { asm volatile("mfence" ::: "memory"); }
inline static void rmb(void) { asm volatile("lfence" ::: "memory"); }
inline static void wmb(void) { asm volatile("sfence" ::: "memory"); }
#endif

#define NOP1	asm  volatile ("nop")
#define NOP2	asm  volatile ("nop;nop")
#define NOP4	asm  volatile ("nop;nop;nop;nop")
#define NOP8	asm  volatile ("nop;nop;nop;nop;nop;nop;nop;nop")

#ifdef __cplusplus
}
#endif

#endif
