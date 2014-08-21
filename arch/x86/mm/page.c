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

#include <eduos/stdio.h>
#include <eduos/memory.h>
#include <eduos/errno.h>
#include <eduos/string.h>

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/io.h>

/* Note that linker symbols are not variables, they have no memory
 * allocated for maintaining a value, rather their address is their value. */
extern const void kernel_start;
extern const void kernel_end;

/** This PGD table is initialized in entry.asm */
extern size_t boot_map[PAGE_MAP_ENTRIES];

/** A self-reference enables direct access to all page tables */
static size_t* self[PAGE_LEVELS] = {
		(size_t *) PAGE_MAP_PGT, (size_t *) PAGE_MAP_PGD
};

#define self_child(lvl, vpn)	&self[lvl-1][vpn<<PAGE_MAP_BITS]
#define self_parent(lvl, vpn)	&self[lvl+1][vpn>>PAGE_MAP_BITS]

/** @todo Does't handle huge pages for now 
 *  @todo This will cause a pagefaut if addr isn't mapped! */
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
	long first[PAGE_LEVELS], last[PAGE_LEVELS];	// index boundaries for self-mapping

	for (lvl=0; lvl<PAGE_LEVELS; lvl++) {
		first[lvl] = (vpn         ) >> (lvl * PAGE_MAP_BITS);
		last[lvl]  = (vpn + npages) >> (lvl * PAGE_MAP_BITS);
	}

	/* We start now iterating through the entries
	 * beginning at the root table (PGD) */
	for (lvl=PAGE_LEVELS-1; lvl>=0; lvl--) {
		for (vpn=first[lvl]; vpn<=last[lvl]; vpn++) {
			if (lvl) { /* PML4, PDPT, PGD */	
				if (self[lvl][vpn] & PG_PRESENT) {
					/* There already an existing table which only allows
					 * kernel accesses. We need to copy the table to create
					 * private copy for the user space process */
					if (!(self[lvl][vpn] & PG_USER) && (bits & PG_USER)) {
						size_t phyaddr = get_pages(1);
						if (BUILTIN_EXPECT(!phyaddr, 0))
							return -ENOMEM;

						/* Copy old table contents to new one.
						 * We temporarily use page zero for this
						 * by mapping the new table to this address. */
						page_map(0, phyaddr, 1, PG_RW | PG_PRESENT);
						memcpy(0, self_child(lvl, vpn), PAGE_SIZE);

						/* Update table by replacing address and altering flags */
						self[lvl][vpn] &= ~(PAGE_MASK | PG_GLOBAL);
						self[lvl][vpn] |=     phyaddr | PG_USER;

						/* We only need to flush the self-mapped table.
						 * TLB entries mapped by this table remain valid
						 * because we only made an identical copy. */
						tlb_flush_one_page((size_t) self_child(lvl, vpn));
					}
				}
				else {
					/* There's no table available which covers the region.
					 * Therefore we need to create a new empty table. */
					size_t phyaddr = get_pages(1);
					if (BUILTIN_EXPECT(!phyaddr, 0))
						return -ENOMEM;

					/* Reference the new table in the parent */
					self[lvl][vpn] = phyaddr | bits;

					/* Fill new table with zeros */
					memset(self_child(lvl, vpn), 0, PAGE_SIZE);
				}
			}
			else { /* PGT */
				if (self[lvl][vpn] & PG_PRESENT)
					tlb_flush_one_page(vpn << PAGE_BITS);

				self[lvl][vpn] = phyaddr | bits;
				phyaddr += PAGE_SIZE;
			}
		}
	}

	return 0;
}

int page_unmap(size_t viraddr, size_t npages)
{
	int lvl;
	long vpn = viraddr >> PAGE_BITS;
	long first[PAGE_LEVELS], last[PAGE_LEVELS];	// index boundaries for self-mapping

	for (lvl=0; lvl<PAGE_LEVELS; lvl++) {
		first[lvl] = (vpn         ) >> (lvl * PAGE_MAP_BITS);
		last[lvl]  = (vpn + npages) >> (lvl * PAGE_MAP_BITS);
	}

	/* We start now iterating through the entries
	 * beginning at the root table (PGD) */
	for (lvl=PAGE_LEVELS-1; lvl>=0; lvl--) {
		for (vpn=first[lvl]; vpn<=last[lvl]; vpn++) {
			if (lvl) { /* PML4, PDPT, PGD */	

			}
			else { /* PGT */

			}
		}
	}

	return 0;
}

int page_map_drop(size_t *map)
{
	int lvl;
	long vpn;

	/* We start now iterating through the entries
	 * beginning at the root table (PGD) */
	for (lvl=PAGE_LEVELS-1; lvl>=0; lvl--) {
		for (vpn=0; vpn<PAGE_MAP_ENTRIES; vpn++) {
			if (lvl) { /* PML4, PDPT, PGD */	

			}
			else { /* PGT */

			}
		}
	}

	return 0;
}

int page_map_copy(size_t *dest, size_t *src)
{
	int lvl;
	long vpn;

	/* We start now iterating through the entries
	 * beginning at the root table (PGD) */
	for (lvl=PAGE_LEVELS-1; lvl>=0; lvl--) {
		for (vpn=0; vpn<PAGE_MAP_ENTRIES; vpn++) {
			if (lvl) { /* PML4, PDPT, PGD */	

			}
			else { /* PGT */

			}
		}
	}

	return 0;
}

void page_fault_handler(struct state *s)
{
	size_t viraddr = read_cr2();

	kprintf("Page Fault Exception (%d) at cs:ip = %#x:%#lx, address = %#lx\n",
		s->int_no, s->cs, s->eip, viraddr);

	outportb(0x20, 0x20);

	while(1) HALT;
}

int page_init()
{
	size_t npages;

	// replace default pagefault handler
	irq_uninstall_handler(14);
	irq_install_handler(14, page_fault_handler);

	// map kernel
	npages = PAGE_FLOOR((size_t) &kernel_end - (size_t) &kernel_start) >> PAGE_BITS;
	page_map((size_t) &kernel_start, (size_t) &kernel_start, npages, PG_PRESENT | PG_RW | PG_GLOBAL);

#ifdef CONFIG_VGA
	// map video memory
	page_map(VIDEO_MEM_ADDR, VIDEO_MEM_ADDR, 1, PG_PCD | PG_PRESENT | PG_RW);
#endif

	// flush TLB to apply new mappings
	flush_tlb();

	return 0;
}
