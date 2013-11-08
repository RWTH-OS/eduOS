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

#ifndef __ARCH_STDDEF_H__
#define __ARCH_STDDEF_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long size_t;
typedef long ptrdiff_t;
#ifdef __KERNEL__
typedef long ssize_t;
typedef long off_t;
#endif

typedef unsigned long long uint64_t;
typedef long long int64_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef unsigned char uint8_t;
typedef char int8_t;
typedef unsigned short wchar_t;

#ifndef _WINT_T
#define _WINT_T
typedef unsigned int wint_t;
#endif

/* This defines what the stack looks like after an ISR was running */
struct state {
	/* 
	 * We switched from software- to hardwaree-based multitasking
	 * Therefore, we do not longer save the registers by hand.
         */
	/*unsigned int gs, fs, es, ds; */				/* pushed the segs last */
	unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;		/* pushed by 'pusha' */
	unsigned int int_no, err_code;					/* our 'push byte #' and ecodes do this */
	/*unsigned int eip, cs, eflags, useresp, ss;*/			/* pushed by the processor automatically */
};

#ifdef __cplusplus
}
#endif

#endif
