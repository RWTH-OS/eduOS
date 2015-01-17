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
#include <eduos/stdlib.h>
#include <eduos/string.h>
#include <eduos/stdarg.h>
#include <eduos/spinlock.h>
#include <eduos/fs.h>
#include <asm/atomic.h>
#include <asm/processor.h>
#include <asm/multiboot.h>
#ifdef CONFIG_VGA
#include <asm/vga.h>
#endif
#ifdef CONFIG_UART
#include <asm/uart.h>
#endif

#define NO_EARLY_PRINT		0x00
#define VGA_EARLY_PRINT		0x01
#define UART_EARLY_PRINT	0x02

#ifdef CONFIG_VGA
static uint32_t early_print = VGA_EARLY_PRINT;
#else
static uint32_t early_print = NO_EARLY_PRINT;
#endif
static spinlock_irqsave_t olock = SPINLOCK_IRQSAVE_INIT;
static atomic_int32_t kmsg_counter = ATOMIC_INIT(0);
static unsigned char kmessages[KMSG_SIZE] __attribute__ ((section(".kmsg"))) = {[0 ... KMSG_SIZE-1] = 0x00};

static ssize_t kmsg_read(fildes_t* file, uint8_t* buffer, size_t size)
{
	size_t start, i = 0;

	if (BUILTIN_EXPECT(!buffer, 0))
		return -EINVAL;
	if (BUILTIN_EXPECT(!size, 0))
		return 0;

	if (kmessages[(atomic_int32_read(&kmsg_counter) + 1) % KMSG_SIZE] == 0)
		start = 0;
	else
		start = (atomic_int32_read(&kmsg_counter) + 1) % KMSG_SIZE;

	if (((start + file->offset) % KMSG_SIZE) == atomic_int32_read(&kmsg_counter))
		return 0;
	if (file->offset >= KMSG_SIZE)
		return 0;

	for(i=0; i<size; i++, file->offset++) {
		buffer[i] = kmessages[(start + file->offset) % KMSG_SIZE];
		if (((start + file->offset) % KMSG_SIZE) == atomic_int32_read(&kmsg_counter))
			return i;
	}

	return size;
}

static int kmsg_open(fildes_t* file, const char *name)
{
	return 0;
}

static int kmsg_close(fildes_t* file)
{
	return 0;
}

/* Init Functions */
int kmsg_init(vfs_node_t * node, const char *name)
{
	uint32_t        i, j;
	vfs_node_t*     new_node;
	dir_block_t*    blockdir;
	dirent_t*       dirent;
	block_list_t*   blist;

	if (BUILTIN_EXPECT(!node || !name, 0))
		return -EINVAL;

	if (BUILTIN_EXPECT(node->type != FS_DIRECTORY, 0))
		return -EINVAL;

	if (finddir_fs(node, name))
		return -EINVAL;

	new_node = kmalloc(sizeof(vfs_node_t));
	if (BUILTIN_EXPECT(!new_node, 0))
		return -ENOMEM;

	memset(new_node, 0x00, sizeof(vfs_node_t));
	new_node->type = FS_CHARDEVICE;
	new_node->open = &kmsg_open;
	new_node->close = &kmsg_close;
	new_node->read = &kmsg_read;
	new_node->write = NULL;
	spinlock_init(&new_node->lock);

	blist = &node->block_list;
	do {
		for (i = 0; i < MAX_DATABLOCKS; i++) {
			if (blist->data[i]) {
				blockdir = (dir_block_t *) blist->data[i];
				for (j = 0; j < MAX_DIRENTRIES; j++) {
					dirent = &blockdir->entries[j];
					if (!dirent->vfs_node) {
						dirent->vfs_node = new_node;
						strncpy(dirent->name, name, MAX_FNAME);
						return 0;
					}
				}
			}
		}

		if (!blist->next) {
			blist->next = (block_list_t *) kmalloc(sizeof(block_list_t));
			if (blist->next)
				memset(blist->next, 0x00, sizeof(block_list_t));
		}
	} while (blist);

	kfree(new_node);

	return -ENOMEM;
}

int koutput_init(void)
{
#ifdef CONFIG_VGA
	vga_init();
#endif
#ifdef CONFIG_UART
	if (mb_info && (mb_info->flags & MULTIBOOT_INFO_CMDLINE))
		if (!uart_early_init((char*) mb_info->cmdline))
			early_print |= UART_EARLY_PRINT;
#endif

	return 0;
}

int kputchar(int c)
{
	int pos;

	if (early_print != NO_EARLY_PRINT)
		spinlock_irqsave_lock(&olock);

	pos = atomic_int32_inc(&kmsg_counter);
	kmessages[pos % KMSG_SIZE] = (unsigned char) c;

#ifdef CONFIG_VGA
	if (early_print & VGA_EARLY_PRINT)
		vga_putchar(c);
#endif
#ifdef CONFIG_UART
	if (early_print & UART_EARLY_PRINT)
		uart_putchar(c);
#endif

	if (early_print != NO_EARLY_PRINT)
		spinlock_irqsave_unlock(&olock);

	return 1;
}

int kputs(const char *str)
{
	int pos, i, len = strlen(str);

	if (early_print != NO_EARLY_PRINT)
		spinlock_irqsave_lock(&olock);

	for(i=0; i<len; i++) {
		pos = atomic_int32_inc(&kmsg_counter);
		kmessages[pos % KMSG_SIZE] = str[i];
#ifdef CONFIG_VGA
		if (early_print & VGA_EARLY_PRINT)
			vga_putchar(str[i]);
#endif
#ifdef CONFIG_UART
		if (early_print & UART_EARLY_PRINT)
			uart_putchar(str[i]);
#endif
	}

	if (early_print != NO_EARLY_PRINT)
		spinlock_irqsave_unlock(&olock);

	return len;
}

int koutput_add_uart(void)
{
#ifdef CONFIG_UART
	early_print |= UART_EARLY_PRINT;

	return 0;
#else
	return -EINVAL;
#endif
}
