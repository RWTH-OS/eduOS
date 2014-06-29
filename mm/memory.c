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
#include <eduos/stdlib.h>
#include <eduos/spinlock.h>

#include <asm/atomic.h>
#include <asm/multiboot.h>
#include <asm/page.h>


static char stack[MAX_TASKS-1][KERNEL_STACK_SIZE];
static char bitmap[BITMAP_SIZE];

static spinlock_t bitmap_lock = SPINLOCK_INIT;

atomic_int32_t total_pages = ATOMIC_INIT(0);
atomic_int32_t total_allocated_pages = ATOMIC_INIT(0);
atomic_int32_t total_available_pages = ATOMIC_INIT(0);

void* create_stack(tid_t id)
{
	// idle task uses stack, which is defined in entry.asm
	if (BUILTIN_EXPECT(!id, 0))
		return NULL;
	// do we have a valid task id?
	if (BUILTIN_EXPECT(id >= MAX_TASKS, 0))
		return NULL;

	return (void*) stack[id-1];
}

inline static int page_marked(size_t i)
{
	size_t index = i >> 3;
	size_t mod = i & 0x7;

	return  (bitmap[index] & (1 << mod));
}

inline static void page_set_mark(size_t i)
{
	size_t index = i >> 3;
	size_t mod = i & 0x7;

	bitmap[index] = bitmap[index] | (1 << mod);
}

inline static void page_clear_mark(size_t i)
{
	size_t index = i / 8;
	size_t mod = i % 8;

	bitmap[index] = bitmap[index] & ~(1 << mod);
}

size_t get_pages(size_t npages)
{
	size_t cnt, off;

	if (BUILTIN_EXPECT(!npages, 0))
		return 0;
	if (BUILTIN_EXPECT(npages > atomic_int32_read(&total_available_pages), 0))
		return 0;

	spinlock_lock(&bitmap_lock);

	off = 1;
	while (off <= BITMAP_SIZE*8 - npages) {
		for (cnt=0; cnt<npages; cnt++) {
			if (page_marked(off+cnt))
				goto next;
		}

		for (cnt=0; cnt<npages; cnt++) {
			page_set_mark(off+cnt);
		}

		spinlock_unlock(&bitmap_lock);

		atomic_int32_add(&total_allocated_pages, npages);
		atomic_int32_sub(&total_available_pages, npages);

		return off << PAGE_BITS;

next:		off += cnt+1;
	}

	spinlock_unlock(&bitmap_lock);

	return 0;
}

int put_pages(size_t phyaddr, size_t npages)
{
	size_t i, ret = 0;
	size_t base = phyaddr >> PAGE_BITS;

	if (BUILTIN_EXPECT(!phyaddr, 0))
		return -EINVAL;
	if (BUILTIN_EXPECT(!npages, 0))
		return -EINVAL;

	spinlock_lock(&bitmap_lock);

	for (i=0; i<npages; i++) {
		if (page_marked(base+i)) {
			page_clear_mark(base+i);
			ret++;
		}
	}

	spinlock_unlock(&bitmap_lock);

	atomic_int32_sub(&total_allocated_pages, ret);
	atomic_int32_add(&total_available_pages, ret);

	return ret;
}
