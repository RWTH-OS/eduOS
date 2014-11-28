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
/**
 * This is a 32/64 bit portable paging implementation for the x86 architecture
 * using self-referenced page tables.
 * See http://www.noteblok.net/2014/06/14/bachelor/ for a detailed description.
 */

#include <eduos/stdio.h>
#include <eduos/memory.h>
#include <eduos/errno.h>
#include <eduos/string.h>
#include <eduos/spinlock.h>

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/multiboot.h>

/* Note that linker symbols are not variables, they have no memory
 * allocated for maintaining a value, rather their address is their value. */
extern const void kernel_start;
extern const void kernel_end;

/** Lock for kernel space page tables */
static spinlock_t kslock = SPINLOCK_INIT;

/** This PGD table is initialized in entry.asm */
extern size_t boot_map[PAGE_MAP_ENTRIES];

/** A self-reference enables direct access to all page tables */
static size_t* self[PAGE_LEVELS] = {
	(size_t *) 0xFFC00000,
	(size_t *) 0xFFFFF000
};

/** An other self-reference for page_map_copy() */
static size_t * other[PAGE_LEVELS] = {
	(size_t *) 0xFF800000,
	(size_t *) 0xFFFFE000
};

/* Addresses of child/parent tables */
#define  CHILD(map, lvl, vpn)	&map[lvl-1][vpn<<PAGE_MAP_BITS]
#define PARENT(map, lvl, vpn)	&map[lvl+1][vpn>>PAGE_MAP_BITS]

size_t page_virt_to_phys(size_t addr)
{
	size_t vpn   = addr >> PAGE_BITS;	// virtual page number
	size_t entry = self[0][vpn];		// page table entry
	size_t off   = addr  & ~PAGE_MASK;	// offset within page
	size_t phy   = entry &  PAGE_MASK;	// physical page frame number

	return phy | off;
}

int page_map(size_t viraddr, size_t phyaddr, size_t npages, size_t bits)
{
	int lvl;
	long vpn = viraddr >> PAGE_BITS;
	long first[PAGE_LEVELS], last[PAGE_LEVELS];

	/* Calculate index boundaries for page map traversal */
	for (lvl=0; lvl<PAGE_LEVELS; lvl++) {
		first[lvl] = (vpn         ) >> (lvl * PAGE_MAP_BITS);
		last[lvl]  = (vpn+npages-1) >> (lvl * PAGE_MAP_BITS);
	}

	spinlock_lock(&kslock);

	/* Start iterating through the entries
	 * beginning at the root table (PGD or PML4) */
	for (lvl=PAGE_LEVELS-1; lvl>=0; lvl--) {
		for (vpn=first[lvl]; vpn<=last[lvl]; vpn++) {
			if (lvl) { /* PML4, PDPT, PGD */
				if (!(self[lvl][vpn] & PG_PRESENT)) {
					/* There's no table available which covers the region.
					 * Therefore we need to create a new empty table. */
					size_t phyaddr = get_pages(1);
					if (BUILTIN_EXPECT(!phyaddr, 0)) {
						spinlock_unlock(&kslock);
						return -ENOMEM;
					}

					/* Reference the new table within its parent */
					self[lvl][vpn] = phyaddr | bits;

					/* Fill new table with zeros */
					memset(CHILD(self, lvl, vpn), 0, PAGE_SIZE);
				}
			}
			else { /* PGT */
				if (self[lvl][vpn] & PG_PRESENT)
					/* There's already a page mapped at this address.
					 * We have to flush a single TLB entry. */
					tlb_flush_one_page(vpn << PAGE_BITS);

				self[lvl][vpn] = phyaddr | bits;
				phyaddr += PAGE_SIZE;
			}
		}
	}

	spinlock_unlock(&kslock);

	return 0;
}

/** Tables are freed by page_map_drop() */
int page_unmap(size_t viraddr, size_t npages)
{
	long vpn, start = viraddr >> PAGE_BITS;
	long end = start + npages;

	spinlock_lock(&kslock);

        /* Start iterating through the entries.
         * Only the PGT entries are removed. Tables remain allocated. */
	for (vpn=start; vpn<end; vpn++)
		self[0][vpn] = 0;

	spinlock_unlock(&kslock);

	return 0;
}

int page_map_drop()
{
	void traverse(int lvl, long vpn) {
		kprintf("traverse(lvl=%d, vpn=%#lx)\n", lvl, vpn);

		long stop;
		for (stop=vpn+PAGE_MAP_ENTRIES; vpn<stop; vpn++) {
			if (self[lvl][vpn] & PG_PRESENT) {
				if (self[lvl][vpn] & PG_BOOT)
					continue;

				// ost-order traversal
				if (lvl > 1)
					traverse(lvl-1, vpn<<PAGE_MAP_BITS);

				kprintf("%#lx, ", self[lvl][vpn] & PAGE_MASK);
				//put_page(self[lvl][vpn] & PAGE_MASK);

				atomic_int32_dec(&current_task->user_usage);
			}
		}
	}

	spinlock_irqsave_lock(&current_task->page_lock);

	traverse(PAGE_LEVELS-1, 0);

	spinlock_irqsave_unlock(&current_task->page_lock);

	return 0;
}

int page_map_copy(size_t dest)
{
	int traverse(int lvl, long vpn) {
		long stop;
		for (stop=vpn+PAGE_MAP_ENTRIES; vpn<stop; vpn++) {
			if (self[lvl][vpn] & PG_PRESENT) {
				size_t phyaddr = get_pages(1);
				if (BUILTIN_EXPECT(phyaddr, 0))
					return -ENOMEM;

		                other[lvl][vpn]  = phyaddr;
				other[lvl][vpn] |= self[lvl][vpn] & ~PAGE_MASK;

				memcpy(CHILD(other, lvl, vpn), CHILD(self, lvl, vpn), PAGE_SIZE);

				// pre-order traversal
				if (lvl)
					traverse(lvl-1, vpn<<PAGE_MAP_BITS);
			}
		}

		return 0;
	}

	spinlock_lock(&kslock);

	// create another temporary self-reference
	self[PAGE_LEVELS-1][PAGE_MAP_ENTRIES-2] = dest | PG_PRESENT | PG_RW;

	traverse(PAGE_LEVELS-1, 0);

	// remove temporary self-reference
	self[PAGE_LEVELS-1][PAGE_MAP_ENTRIES-2] = 0;

	spinlock_unlock(&kslock);

	return 0;
}

void page_fault_handler(struct state *s)
{
	size_t viraddr = read_cr2();

        kprintf("Page Fault Exception (%d) at cs:ip = %#x:%#lx, task = %u, addr = %#lx, error = %#x [ %s %s %s %s %s ]\n",
		s->int_no, s->cs, s->eip, current_task->id, viraddr, s->error,
		(s->error & 0x4) ? "user" : "supervisor",
		(s->error & 0x10) ? "instruction" : "data",
		(s->error & 0x2) ? "write" : ((s->error & 0x10) ? "fetch" : "read"),
		(s->error & 0x1) ? "protection" : "not present",
		(s->error & 0x8) ? "reserved bit" : "\b");

	while(1) HALT;
}

int page_init()
{
	size_t addr, npages;
	int i;

	/* Replace default pagefault handler */
	irq_uninstall_handler(14);
	irq_install_handler(14, page_fault_handler);

	/* Map kernel */
	addr = (size_t) &kernel_start;
	npages = PAGE_FLOOR((size_t) &kernel_end - (size_t) &kernel_start) >> PAGE_BITS;
	page_map(addr, addr, npages, PG_PRESENT | PG_RW | PG_GLOBAL);

#ifdef CONFIG_VGA
	/* Map video memory */
	page_map(VIDEO_MEM_ADDR, VIDEO_MEM_ADDR, 1, PG_PRESENT | PG_RW | PG_PCD);
#endif

	/* Map multiboot information and modules */
	if (mb_info) {
		addr = (size_t) mb_info & PAGE_MASK;
		npages = PAGE_FLOOR(sizeof(*mb_info)) >> PAGE_BITS;
		page_map(addr, addr, npages, PG_PRESENT | PG_GLOBAL);

		if (mb_info->flags & MULTIBOOT_INFO_MODS) {
			addr = mb_info->mods_addr;
			npages = PAGE_FLOOR(mb_info->mods_count*sizeof(multiboot_module_t)) >> PAGE_BITS;
			page_map(addr, addr, npages, PG_PRESENT | PG_GLOBAL);

			multiboot_module_t* mmodule = (multiboot_module_t*) ((size_t) mb_info->mods_addr);
			for(i=0; i<mb_info->mods_count; i++) {
				addr = mmodule[i].mod_start;
				npages = PAGE_FLOOR(mmodule[i].mod_end - mmodule[i].mod_start) >> PAGE_BITS;
				page_map(addr, addr, npages, PG_PRESENT | PG_USER | PG_GLOBAL);
			}
		}
	}

	/* Unmap bootstrap identity paging (see entry.asm, PG_BOOT) */
	for (i=0; i<PAGE_MAP_ENTRIES; i++) {
		if (self[0][i] & PG_BOOT) {
			self[0][i] = 0;
			tlb_flush_one_page(i << PAGE_BITS);
		}
	}

	return 0;
}
