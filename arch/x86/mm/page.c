/*
 * Copyright (c) 2010, Stefan Lankes, RWTH Aachen University
 *               2014, Steffen Vogel, RWTH Aachen University
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
 * using self-referenced page tables	i.
 * See http://www.noteblok.net/2014/06/14/bachelor/ for a detailed description.
 * 
 * @author Steffen Vogel <steffen.vogel@rwth-aachen.de>
 */

#include <eduos/stdio.h>
#include <eduos/memory.h>
#include <eduos/errno.h>
#include <eduos/string.h>
#include <eduos/spinlock.h>

#include <asm/irq.h>
#include <asm/page.h>
#include <asm/multiboot.h>

/* Note that linker symbols are not variables, they have no memory
 * allocated for maintaining a value, rather their address is their value. */
extern const void kernel_start;
//extern const void kernel_end;

/// This page is reserved for copying
#define PAGE_TMP		(PAGE_FLOOR((size_t) &kernel_start) - PAGE_SIZE)

/** Lock for kernel space page tables */
static spinlock_t kslock = SPINLOCK_INIT;

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

/// Mapping of self referenced page map (at the end of the VAS)
// TODO: find a more generic initialization
static size_t* const current_map = (size_t*) (1 * 0xFFFFF000);

/** @brief Get the base address of the child table
 *
 * @param entry The parent entry
 * @return The child entry
 */
static inline size_t* get_child_entry(size_t *entry)
{
	size_t child = (size_t) entry;

	child <<= PAGE_MAP_BITS;

	return (size_t*) CANONICAL(child);
}

/** @brief Get the base address of the parent entry
 *
 * @param entry The child entry
 * @return The parent entry
 */
static inline size_t* get_parent_entry(size_t *entry)
{
	ssize_t parent = (size_t) entry;

	parent >>= PAGE_MAP_BITS;
	parent |= (size_t) self[0];
	parent &= ~(sizeof(size_t) - 1); // align to page_entry_t

	return (size_t*) CANONICAL(parent);
}

/** @brief Get the corresponding page map entry to a given virtual address
 *
 * Please note: this implementation requires that the tables are mapped
 * at the end of VAS!
 */
static inline size_t* virt_to_entry(ssize_t addr, int level)
{
	addr >>= PAGE_MAP_BITS;
	addr |= (size_t) self[0]; //TODO: PAGE_MAP_PGT;

	addr >>= level * PAGE_MAP_BITS;
	addr &= ~(sizeof(size_t) - 1); // align to page_entry_t

	return (size_t*) CANONICAL(addr);
}

/** @brief Get the corresponding virtual address to a page map entry */
static inline size_t entry_to_virt(size_t* entry, int level)
{
	size_t addr = (size_t) entry;

	addr <<= (level+1) * PAGE_MAP_BITS;

	return CANONICAL(addr);
}

/** @brief Update page table bits (PG_*) by using arch independent flags (MAP_*) */
static inline size_t page_bits(int flags)
{
#ifdef CONFIG_X86_32
	size_t bits = PG_PRESENT | PG_RW | PG_GLOBAL;
#elif defined(CONFIG_X86_64)
	size_t bits = PG_PRESENT | PG_RW | PG_XD | PG_GLOBAL;
#endif

	if (flags & MAP_NO_ACCESS)	bits &= ~PG_PRESENT;
	if (flags & MAP_READ_ONLY)	bits &= ~PG_RW;
#ifdef CONFIG_X86_64
	if (flags & MAP_CODE)		bits &= ~PG_XD;
#endif
	if (flags & MAP_USER_SPACE)	bits &= ~PG_GLOBAL;
	if (flags & MAP_USER_SPACE)	bits |= PG_USER;
	if (flags & MAP_WT)			bits |= PG_PWT;
	if (flags & MAP_NO_CACHE)	bits |= PG_PCD;

	return bits;
}

size_t virt_to_phys(size_t addr)
{
	size_t vpn   = addr >> PAGE_BITS;	// virtual page number
	size_t entry = self[0][vpn];		// page table entry
	size_t off   = addr  & ~PAGE_MASK;	// offset within page
	size_t phy   = entry &  PAGE_MASK;	// physical page frame number

	return phy | off;
}

int page_map(size_t viraddr, size_t phyaddr, size_t npages, size_t bits)
{
	int lvl, ret = -ENOMEM;
	long vpn = viraddr >> PAGE_BITS;
	long first[PAGE_LEVELS], last[PAGE_LEVELS];

	/* Calculate index boundaries for page map traversal */
	for (lvl=0; lvl<PAGE_LEVELS; lvl++) {
		first[lvl] = (vpn         ) >> (lvl * PAGE_MAP_BITS);
		last[lvl]  = (vpn+npages-1) >> (lvl * PAGE_MAP_BITS);
	}

	/** @todo: might not be sufficient! */
	if (bits & PG_USER)
		spinlock_irqsave_lock(&current_task->page_lock);
	else
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
					if (BUILTIN_EXPECT(!phyaddr, 0))
						goto out;
					
					if (bits & PG_USER)
						atomic_int32_inc(&current_task->user_usage);

					/* Reference the new table within its parent */
					self[lvl][vpn] = phyaddr | bits | PG_PRESENT;

					/* Fill new table with zeros */
					memset(&self[lvl-1][vpn<<PAGE_MAP_BITS], 0, PAGE_SIZE);
				}
			}
			else { /* PGT */
				if (self[lvl][vpn] & PG_PRESENT)
					/* There's already a page mapped at this address.
					 * We have to flush a single TLB entry. */
					tlb_flush_one_page(vpn << PAGE_BITS);

				self[lvl][vpn] = phyaddr | bits | PG_PRESENT;
				phyaddr += PAGE_SIZE;
			}
		}
	}

	ret = 0;
out:
	if (bits & PG_USER)
		spinlock_irqsave_unlock(&current_task->page_lock);
	else
		spinlock_unlock(&kslock);

	return ret;
}

/** Tables are freed by page_map_drop() */
int page_unmap(size_t viraddr, size_t npages)
{
	/* We aquire both locks for kernel and task tables
	 * as we dont know to which the region belongs. */
	spinlock_irqsave_lock(&current_task->page_lock);
	spinlock_lock(&kslock);

	/* Start iterating through the entries.
	 * Only the PGT entries are removed. Tables remain allocated. */
	size_t vpn, start = viraddr>>PAGE_BITS;
	for (vpn=start; vpn<start+npages; vpn++)
		self[0][vpn] = 0;

	spinlock_irqsave_unlock(&current_task->page_lock);
	spinlock_unlock(&kslock);

	/* This can't fail because we don't make checks here */
	return 0;
}

int page_map_drop()
{
	void traverse(int lvl, long vpn) {
		long stop;
		for (stop=vpn+PAGE_MAP_ENTRIES; vpn<stop; vpn++) {
			if ((self[lvl][vpn] & PG_PRESENT) && (self[lvl][vpn] & PG_USER)) {
				/* Post-order traversal */
				if (lvl)
					traverse(lvl-1, vpn<<PAGE_MAP_BITS);

				put_pages(self[lvl][vpn] & PAGE_MASK, 1);
				atomic_int32_dec(&current_task->user_usage);
			}
		}
	}

	spinlock_irqsave_lock(&current_task->page_lock);

	traverse(PAGE_LEVELS-1, 0);

	spinlock_irqsave_unlock(&current_task->page_lock);

	/* This can't fail because we don't make checks here */
	return 0;
}

int page_map_copy(task_t *dest)
{
	int traverse(int lvl, long vpn) {
		long stop;
		for (stop=vpn+PAGE_MAP_ENTRIES; vpn<stop; vpn++) {
			if (self[lvl][vpn] & PG_PRESENT) {
				if (self[lvl][vpn] & PG_USER) {
					size_t phyaddr = get_pages(1);
					if (BUILTIN_EXPECT(!phyaddr, 0))
						return -ENOMEM;
					
					atomic_int32_inc(&dest->user_usage);

					other[lvl][vpn] = phyaddr | (self[lvl][vpn] & ~PAGE_MASK);
					if (lvl) /* PML4, PDPT, PGD */
						traverse(lvl-1, vpn<<PAGE_MAP_BITS); /* Pre-order traversal */
					else { /* PGT */
						page_map(PAGE_TMP, phyaddr, 1, PG_RW);
						memcpy((void*) PAGE_TMP, (void*) (vpn<<PAGE_BITS), PAGE_SIZE);
					}
				}
				else if (self[lvl][vpn] & PG_SELF)
					other[lvl][vpn] = 0;
				else
					other[lvl][vpn] = self[lvl][vpn];
			}
			else
				other[lvl][vpn] = 0;
		}
		return 0;
	}

	spinlock_irqsave_lock(&current_task->page_lock);
	self[PAGE_LEVELS-1][PAGE_MAP_ENTRIES-2] = dest->page_map | PG_PRESENT | PG_SELF | PG_RW;

	int ret = traverse(PAGE_LEVELS-1, 0);

	other[PAGE_LEVELS-1][PAGE_MAP_ENTRIES-1] = dest->page_map | PG_PRESENT | PG_SELF | PG_RW;
	self [PAGE_LEVELS-1][PAGE_MAP_ENTRIES-2] = 0;
	spinlock_irqsave_unlock(&current_task->page_lock);

	/* Flush TLB entries of 'other' self-reference */
	flush_tlb();

	return ret;
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

size_t map_region(size_t viraddr, size_t phyaddr, uint32_t npages, uint32_t flags)
{
	task_t* task = current_task;
	size_t* first[PAGE_LEVELS];
	size_t* last[PAGE_LEVELS];

	// TODO: this behaviour should be deprecated
	if (!viraddr) {
		int vma_flags = VMA_HEAP;
		if (flags & MAP_USER_SPACE)
			vma_flags |= VMA_USER;

		viraddr = vma_alloc(npages * PAGE_SIZE, vma_flags);
	}

	size_t bits = page_bits(flags);
	size_t start = viraddr;
	size_t end = start + npages * PAGE_SIZE;

	int traverse(int level, size_t* entry) {
		size_t* stop = entry + PAGE_MAP_ENTRIES;
		for (; entry != stop; entry++) {
			if (entry < last[level] && entry >= first[level]) {
				if (level) { // PGD, PDPT, PML4..
					if (*entry & PG_PRESENT) {
						if ((flags & MAP_USER_SPACE) && !(*entry & PG_USER)) {
							/* We are altering entries which cover
							 * the kernel. So before changing them we need to
							 * make a private copy for the task  */
							size_t phyaddr = get_page();
							if (BUILTIN_EXPECT(!phyaddr, 0))
								return -ENOMEM;

							atomic_int32_inc(&task->user_usage);

							copy_page(phyaddr, *entry & PAGE_MASK);
							*entry = phyaddr | (*entry & ~PAGE_MASK);
							*entry &= ~PG_GLOBAL;
							*entry |= PG_USER;

							/* We just need to flush the table itself.
							 * TLB entries for the kernel remain valid
							 * because we've not changed them. */
							tlb_flush_one_page(entry_to_virt(entry, 0));
						}
					}
					else {
						/* Theres no page map table available
						 * which covers the region. Therefore we will create a
						 * new table. */
						size_t phyaddr = get_page();
						if (BUILTIN_EXPECT(!phyaddr, 0))
							return -ENOMEM;

						if (flags & MAP_USER_SPACE)
							atomic_int32_inc(&task->user_usage);

						*entry = phyaddr | bits;

						memset(get_child_entry(entry), 0x00, PAGE_SIZE); // fill with zeros
					}

					// do "pre-order" traversal if no hugepage
					if (!(*entry & PG_PSE)) {
						int ret = traverse(level-1, get_child_entry(entry));
						if (ret < 0)
							return ret;
					}
				}
				else { // PGT
					if ((*entry & PG_PRESENT) && !(flags & MAP_REMAP))
						return -EINVAL;

					*entry = phyaddr | bits;

					if (flags & MAP_USER_SPACE)
						atomic_int32_inc(&task->user_usage);

					if (flags & MAP_REMAP)
						tlb_flush_one_page(entry_to_virt(entry, level));

					phyaddr += PAGE_SIZE;
				}
			}
		}

		return 0;
	}

	kprintf("map_region: map %u pages from %#lx to %#lx with flags: %#x\n", npages, viraddr, phyaddr, flags); // TODO: remove

	if (BUILTIN_EXPECT(!task || !task->page_map, 0))
		return 0;

	// calc page tree boundaries
	int i;
	for (i=0; i<PAGE_LEVELS; i++) {
		first[i] = virt_to_entry(start, i);
		last[i] = virt_to_entry(end - 1, i) + 1; // exclusive
	}

	// lock tables
	if (start < KERNEL_SPACE)
		spinlock_lock(&kslock);
	if (end >= KERNEL_SPACE)
		spinlock_irqsave_lock(&task->page_lock);

	int ret = traverse(PAGE_LEVELS-1, current_map);

	// unlock tables
	if (start < KERNEL_SPACE)
		spinlock_unlock(&kslock);
	if (end >= KERNEL_SPACE)
		spinlock_irqsave_unlock(&task->page_lock);

	return (ret) ? 0 : viraddr;
}

int unmap_region(size_t viraddr, uint32_t npages)
{
	task_t* task = current_task;
	size_t* first[PAGE_LEVELS];
	size_t* last[PAGE_LEVELS];

	size_t start = viraddr;
	size_t end = start + npages * PAGE_SIZE;

	kprintf("unmap_region: unmap %u pages from %#lx\n", npages, viraddr); // TODO: remove

	/** @return number of page table entries which a present */
	int traverse(int level, size_t* entry) {
		int used = 0;
		size_t* stop = entry + PAGE_MAP_ENTRIES;
		for (; entry != stop; entry++) {
			if (entry < last[level] && entry >= first[level]) {
				if (level) { // PGD, PDPT, PML4
					if ((*entry & PG_PRESENT) && !(*entry & PG_PSE)) {
						// do "post-order" traversal if table is present and no hugepage
						if (traverse(level-1, get_child_entry(entry)))
							used++;
						else { // child table is empty => delete it
							*entry &= ~PG_PRESENT;
							tlb_flush_one_page(entry_to_virt(entry, 0));

							if (*entry & PG_USER) {
								if (put_page(*entry & PAGE_MASK))
									atomic_int32_dec(&task->user_usage);
							}
						}
					}
				}
				else { // PGT
					*entry &= ~PG_PRESENT;

					tlb_flush_one_page(entry_to_virt(entry, level));

					if (*entry & PG_USER)
						atomic_int32_dec(&task->user_usage);
				}
			}
			else {
				if (*entry & PG_PRESENT)
					used++;
			}
		}

		return used;
	}

	if (BUILTIN_EXPECT(!task || !task->page_map, 0))
		return 0;

	// calc page tree boundaries
	int i;
	for (i=0; i<PAGE_LEVELS; i++) {
		first[i] = virt_to_entry(start, i);
		last[i] = virt_to_entry(end - 1, i) + 1; // exclusive
	}

	// lock tables
	if (start < KERNEL_SPACE)
		spinlock_lock(&kslock);
	if (end >= KERNEL_SPACE)
		spinlock_irqsave_lock(&task->page_lock);

	traverse(PAGE_LEVELS-1, current_map);

	// unlock tables
	if (start < KERNEL_SPACE)
		spinlock_unlock(&kslock);
	if (end > KERNEL_SPACE)
		spinlock_irqsave_unlock(&task->page_lock);

	return 0;
}

int page_init(void)
{
	size_t addr, npages;
	int i;

	/* Replace default pagefault handler */
	irq_uninstall_handler(14);
	irq_install_handler(14, page_fault_handler);

	/* Map multiboot information and modules */
	if (mb_info) {
		addr = (size_t) mb_info & PAGE_MASK;
		npages = PAGE_FLOOR(sizeof(*mb_info)) >> PAGE_BITS;
		page_map(addr, addr, npages, PG_GLOBAL);

		if (mb_info->flags & MULTIBOOT_INFO_MODS) {
			addr = mb_info->mods_addr;
			npages = PAGE_FLOOR(mb_info->mods_count*sizeof(multiboot_module_t)) >> PAGE_BITS;
			page_map(addr, addr, npages, PG_GLOBAL);

			multiboot_module_t* mmodule = (multiboot_module_t*) ((size_t) mb_info->mods_addr);
			for(i=0; i<mb_info->mods_count; i++) {
				addr = mmodule[i].mod_start;
				npages = PAGE_FLOOR(mmodule[i].mod_end - mmodule[i].mod_start) >> PAGE_BITS;
				page_map(addr, addr, npages, PG_GLOBAL);
			}
		}
	}

	/* Flush TLB to adopt changes above */
	//flush_tlb();

	return 0;
}
