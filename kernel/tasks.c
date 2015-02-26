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
#include <eduos/stdio.h>
#include <eduos/string.h>
#include <eduos/tasks.h>
#include <eduos/tasks_types.h>
#include <eduos/spinlock.h>
#include <eduos/errno.h>
#include <eduos/syscall.h>
#include <eduos/memory.h>
#include <eduos/fs.h>
#include <eduos/vma.h>
#include <asm/elf.h>
#include <asm/page.h>

/** @brief Array of task structures (aka PCB)
 *
 * A task's id will be its position in this array.
 */
static task_t task_table[MAX_TASKS] = { \
		[0]                 = {0, TASK_IDLE, NULL, NULL, 0, 0, SPINLOCK_IRQSAVE_INIT, SPINLOCK_INIT, NULL, NULL, ATOMIC_INIT(0), NULL, NULL}, \
		[1 ... MAX_TASKS-1] = {0, TASK_INVALID, NULL, NULL, 0, 0, SPINLOCK_IRQSAVE_INIT, SPINLOCK_INIT, NULL, NULL,ATOMIC_INIT(0), NULL, NULL}};

static spinlock_irqsave_t table_lock = SPINLOCK_IRQSAVE_INIT;

static readyqueues_t readyqueues = {task_table+0, NULL, 0, 0, {[0 ... MAX_PRIO-2] = {NULL, NULL}}, SPINLOCK_IRQSAVE_INIT};

task_t* current_task = task_table+0;
extern const void boot_stack;

/** @brief helper function for the assembly code to determine the current task
 * @return Pointer to the task_t structure of current task
 */
task_t* get_current_task(void)
{
	return current_task;
}

uint32_t get_highest_priority(void)
{
	return msb(readyqueues.prio_bitmap);
}

int multitasking_init(void)
{
	if (BUILTIN_EXPECT(task_table[0].status != TASK_IDLE, 0)) {
		kputs("Task 0 is not an idle task\n");
		return -ENOMEM;
	}

	task_table[0].prio = IDLE_PRIO;
	task_table[0].stack = (void*) &boot_stack;
	task_table[0].page_map = read_cr3();

	// register idle task
	register_task();

	return 0;
}

void finish_task_switch(void)
{
	task_t* old;
	uint8_t prio;

	spinlock_irqsave_lock(&readyqueues.lock);

	if ((old = readyqueues.old_task) != NULL) {
		if (old->status == TASK_INVALID) {
			old->stack = NULL;
			old->last_stack_pointer = NULL;
			readyqueues.old_task = NULL;
		} else {
			prio = old->prio;
			if (!readyqueues.queue[prio-1].first) {
				old->next = old->prev = NULL;
				readyqueues.queue[prio-1].first = readyqueues.queue[prio-1].last = old;
			} else {
				old->next = NULL;
				old->prev = readyqueues.queue[prio-1].last;
				readyqueues.queue[prio-1].last->next = old;
				readyqueues.queue[prio-1].last = old;
			}
			readyqueues.old_task = NULL;
			readyqueues.prio_bitmap |= (1 << prio);
		}
	}

	spinlock_irqsave_unlock(&readyqueues.lock);

	if (current_task->heap)
		kfree(current_task->heap);
}

/** @brief A procedure to be called by
 * procedures which are called by exiting tasks. */
static void NORETURN do_exit(int arg)
{
	task_t* curr_task = current_task;

	kprintf("Terminate task: %u, return value %d\n", curr_task->id, arg);

	page_map_drop();

	// decrease the number of active tasks
	spinlock_irqsave_lock(&readyqueues.lock);
	readyqueues.nr_tasks--;
	spinlock_irqsave_unlock(&readyqueues.lock);

	curr_task->status = TASK_FINISHED;
	reschedule();

	kprintf("Kernel panic: scheduler found no valid task\n");
	while(1) {
		HALT;
	}
}

/** @brief A procedure to be called by kernel tasks */
void NORETURN leave_kernel_task(void) {
	int result;

	result = 0; //get_return_value();
	do_exit(result);
}

/** @brief To be called by the systemcall to exit tasks */
void NORETURN sys_exit(int arg) {
	do_exit(arg);
}

/** @brief Aborting a task is like exiting it with result -1 */
void NORETURN abort(void) {
	do_exit(-1);
}

/** @brief Create a task with a specific entry point
 *
 * @todo Dont aquire table_lock for the whole task creation.
 *
 * @param id Pointer to a tid_t struct were the id shall be set
 * @param ep Pointer to the function the task shall start with
 * @param arg Arguments list
 * @param prio Desired priority of the new task
 * @param core_id Start the new task on the core with this id
 *
 * @return
 * - 0 on success
 * - -ENOMEM (-12) or -EINVAL (-22) on failure
 */
static int create_task(tid_t* id, entry_point_t ep, void* arg, uint8_t prio)
{
	int ret = -ENOMEM;
	uint32_t i;

	if (BUILTIN_EXPECT(!ep, 0))
		return -EINVAL;
	if (BUILTIN_EXPECT(prio == IDLE_PRIO, 0))
		return -EINVAL;
	if (BUILTIN_EXPECT(prio > MAX_PRIO, 0))
		return -EINVAL;

	spinlock_irqsave_lock(&table_lock);

	for(i=0; i<MAX_TASKS; i++) {
		if (task_table[i].status == TASK_INVALID) {
			task_table[i].id = i;
			task_table[i].status = TASK_READY;
			task_table[i].last_stack_pointer = NULL;
			task_table[i].stack = create_stack(i);
			task_table[i].prio = prio;
			spinlock_init(&task_table[i].vma_lock);
			task_table[i].vma_list = NULL;
			task_table[i].heap = NULL;

			spinlock_irqsave_init(&task_table[i].page_lock);
			atomic_int32_set(&task_table[i].user_usage, 0);

			/* Allocated new PGD or PML4 and copy page table */
			task_table[i].page_map = get_pages(1);
			if (BUILTIN_EXPECT(!task_table[i].page_map, 0))
				goto out;

			/* Copy page tables & user frames of current task to new one */
			page_map_copy(&task_table[i]);

			if (id)
				*id = i;

			ret = create_default_frame(task_table+i, ep, arg);

			// add task in the readyqueues
			spinlock_irqsave_lock(&readyqueues.lock);
			readyqueues.prio_bitmap |= (1 << prio);
			readyqueues.nr_tasks++;
			if (!readyqueues.queue[prio-1].first) {
				task_table[i].next = task_table[i].prev = NULL;
				readyqueues.queue[prio-1].first = task_table+i;
				readyqueues.queue[prio-1].last = task_table+i;
			} else {
				task_table[i].prev = readyqueues.queue[prio-1].last;
				task_table[i].next = NULL;
				readyqueues.queue[prio-1].last->next = task_table+i;
				readyqueues.queue[prio-1].last = task_table+i;
			}
			spinlock_irqsave_unlock(&readyqueues.lock);
			break;
		}
	}

out:
	spinlock_irqsave_unlock(&table_lock);

	return ret;
}

int create_kernel_task(tid_t* id, entry_point_t ep, void* args, uint8_t prio)
{
	if (prio > MAX_PRIO)
		prio = NORMAL_PRIO;

	return create_task(id, ep, args, prio);
}

#define MAX_ARGS        (PAGE_SIZE - 2*sizeof(int) - sizeof(vfs_node_t*))

/** @brief Structure which keeps all
 * relevant data for a new user task to start */
typedef struct {
	/// Points to the node with the executable in the file system
	vfs_node_t* node;
	/// Argument count
	int argc;
	/// Environment var count
	int envc;
	/// Buffer for env and argv values
	char buffer[MAX_ARGS];
} load_args_t;

/** @brief Internally used function to load tasks with a load_args_t structure
 * keeping all the information needed to launch.
 *
 * This is where the serious loading action is done.
 */
static int load_task(load_args_t* largs)
{
	uint32_t i, offset, idx;
	uint32_t addr, npages, flags;
	size_t stack = 0, heap = 0;
	elf_header_t header;
	elf_program_header_t prog_header;
	//elf_section_header_t sec_header;
	///!!! kfree is missing!
	fildes_t *file = kmalloc(sizeof(fildes_t));
	file->offset = 0;
	file->flags = 0;

	//TODO: init the hole fildes_t struct!
	task_t* curr_task = current_task;
	int err;

	if (!largs)
		return -EINVAL;

	file->node = largs->node;
	if (!file->node)
		return -EINVAL;

	err = read_fs(file, (uint8_t*)&header, sizeof(elf_header_t));
	if (err < 0) {
		kprintf("read_fs failed: %d\n", err);
		return err;
	}

	if (BUILTIN_EXPECT(header.ident.magic != ELF_MAGIC, 0))
		goto invalid;

	if (BUILTIN_EXPECT(header.type != ELF_ET_EXEC, 0))
		goto invalid;

#ifdef CONFIG_X86_32
	if (BUILTIN_EXPECT(header.machine != ELF_EM_386, 0))
		goto invalid;
#elif defined(CONFIG_X86_64)
	if (BUILTIN_EXPECT(header.machine != ELF_EM_X86_64, 0))
		goto invalid;
#else
	goto invalid;
#endif

#ifdef CONFIG_X86_32
	if (BUILTIN_EXPECT(header.ident._class != ELF_CLASS_32, 0))
		goto invalid;
#elif defined(CONFIG_X86_64)
	if (BUILTIN_EXPECT(header.ident._class != ELF_CLASS_64, 0))
		goto invalid;
#else
	goto invalid;
#endif

	if (BUILTIN_EXPECT(header.ident.data != ELF_DATA_2LSB, 0))
		goto invalid;

	if (header.entry <= KERNEL_SPACE)
		goto invalid;

	// interpret program header table
	for (i=0; i<header.ph_entry_count; i++) {
		file->offset = header.ph_offset+i*header.ph_entry_size;
		if (read_fs(file, (uint8_t*)&prog_header, sizeof(elf_program_header_t)) == 0) {
			kprintf("Could not read programm header!\n");
			continue;
		}

		switch(prog_header.type)
		{
		case  ELF_PT_LOAD:  // load program segment
			if (!prog_header.virt_addr)
				continue;

			npages = (prog_header.mem_size >> PAGE_BITS);
			if (prog_header.mem_size & (PAGE_SIZE-1))
				npages++;

			addr = get_pages(npages);

			flags = PG_USER; //TODO: support of XD flags is missing

			// map page frames in the address space of the current task
			if (page_map(prog_header.virt_addr, addr, npages, flags|PG_RW))
				kprintf("Could not map 0x%x at 0x%x\n", addr, prog_header.virt_addr);

			// clear pages
			memset((void*) prog_header.virt_addr, 0x00, npages*PAGE_SIZE);

			// update heap location
			if (heap < prog_header.virt_addr + prog_header.mem_size)
				heap = prog_header.virt_addr + prog_header.mem_size;

			// load program
			file->offset = prog_header.offset;
			read_fs(file, (uint8_t*)prog_header.virt_addr, prog_header.file_size);

			flags = VMA_CACHEABLE;
			if (prog_header.flags & PF_R)
				flags |= VMA_READ;
			if (prog_header.flags & PF_W)
				flags |= VMA_WRITE;
			if (prog_header.flags & PF_X)
				flags |= VMA_EXECUTE;
			vma_add(prog_header.virt_addr, prog_header.virt_addr+npages*PAGE_SIZE-1, flags);

			if (!(prog_header.flags & PF_W))
				page_set_flags(prog_header.virt_addr, npages, flags);
			break;

		case ELF_PT_GNU_STACK: // Indicates stack executability
			// create user-level stack
			npages = DEFAULT_STACK_SIZE >> PAGE_BITS;
			if (DEFAULT_STACK_SIZE & (PAGE_SIZE-1))
				npages++;

			addr = get_pages(npages);
			stack = header.entry*2; // virtual address of the stack

			if (page_map(stack, addr, npages, PG_USER|PG_RW)) {
				kprintf("Could not map stack at 0x%x\n", stack);
				return -ENOMEM;
			}
			memset((void*) stack, 0x00, npages*PAGE_SIZE);

			// create vma regions for the user-level stack
			flags = VMA_CACHEABLE;
			if (prog_header.flags & PF_R)
				flags |= VMA_READ;
			if (prog_header.flags & PF_W)
				flags |= VMA_WRITE;
			if (prog_header.flags & PF_X)
				flags |= VMA_EXECUTE;
			vma_add(stack, stack+npages*PAGE_SIZE-1, flags);
			break;
		}
	}

	// setup heap
	if (!curr_task->heap)
		curr_task->heap = (vma_t*) kmalloc(sizeof(vma_t));

	if (BUILTIN_EXPECT(!curr_task->heap || !heap, 0)) {
		kprintf("load_task: heap is missing!\n");
		return -ENOMEM;
	}

	curr_task->heap->flags = VMA_HEAP|VMA_USER;
	curr_task->heap->start = heap;
	curr_task->heap->end = heap;

	if (BUILTIN_EXPECT(!stack, 0)) {
		kprintf("Stack is missing!\n");
		return -ENOMEM;
	}

	// push strings on the stack
	offset = DEFAULT_STACK_SIZE-8;
	memset((void*) (stack+offset), 0, 4);
	offset -= MAX_ARGS;
	memcpy((void*) (stack+offset), largs->buffer, MAX_ARGS);
	idx = offset;

	// push argv on the stack
	offset -= largs->argc * sizeof(char*);
	for(i=0; i<largs->argc; i++) {
		((char**) (stack+offset))[i] = (char*) (stack+idx);

		while(((char*) stack)[idx] != '\0')
			idx++;
		idx++;
	}

	// push env on the stack
	offset -= (largs->envc+1) * sizeof(char*);
	for(i=0; i<largs->envc; i++) {
		((char**) (stack+offset))[i] = (char*) (stack+idx);

		while(((char*) stack)[idx] != '\0')
			idx++;
		idx++;
	}
	((char**) (stack+offset))[largs->envc] = NULL;

	// push pointer to env
	offset -= sizeof(char**);
	if (!(largs->envc))
		*((char***) (stack+offset)) = NULL;
	else
		*((char***) (stack+offset)) = (char**) (stack + offset + sizeof(char**));

	// push pointer to argv
	offset -= sizeof(char**);
	*((char***) (stack+offset)) = (char**) (stack + offset + 2*sizeof(char**) + (largs->envc+1) * sizeof(char*));

	// push argc on the stack
	offset -= sizeof(int);
	*((int*) (stack+offset)) = largs->argc;

	kfree(largs);

	// clear fpu state => currently not supported
	//curr_task->flags &= ~(TASK_FPU_USED|TASK_FPU_INIT);

	jump_to_user_code(header.entry, stack+offset);

	return 0;

invalid:
	kprintf("Invalid executable!\n");
	kprintf("magic number 0x%x\n", (uint32_t) header.ident.magic);
	kprintf("header type 0x%x\n", (uint32_t) header.type);
	kprintf("machine type 0x%x\n", (uint32_t) header.machine);
	kprintf("elf ident class 0x%x\n", (uint32_t) header.ident._class);
	kprintf("elf identdata 0x%x\n", header.ident.data);
	kprintf("program entry point 0x%lx\n", (size_t) header.entry);

	return -EINVAL;
}

/** @brief This call is used to adapt create_task calls
 * which want to have a start function and argument list */
static int user_entry(void* arg)
{
	int ret;

	finish_task_switch();

	if (BUILTIN_EXPECT(!arg, 0))
		return -EINVAL;

	ret = load_task((load_args_t*) arg);

	kfree(arg);

	return ret;
}

/** @brief Luxus-edition of create_user_task functions. Just call with an exe name
 *
 * @param id Pointer to the tid_t structure which shall be filles
 * @param fname Executable's path and filename
 * @param argv Arguments list
 * @return
 * - 0 on success
 * - -ENOMEM (-12) or -EINVAL (-22) on failure
 */
int create_user_task(tid_t* id, const char* fname, char** argv)
{
	vfs_node_t* node;
	int argc = 0;
	size_t i, buffer_size = 0;
	load_args_t* load_args = NULL;
	char *dest, *src;

	node = findnode_fs((char*) fname);
	if (!node || !(node->type == FS_FILE))
		return -EINVAL;

	// determine buffer size of argv
	if (argv) {
		while (argv[argc]) {
			buffer_size += (strlen(argv[argc]) + 1);
			argc++;
		}
	}

	if (argc <= 0)
		return -EINVAL;
	if (buffer_size >= MAX_ARGS)
		return -EINVAL;

	load_args = kmalloc(sizeof(load_args_t));
	if (BUILTIN_EXPECT(!load_args, 0))
		return -ENOMEM;
	load_args->node = node;
	load_args->argc = argc;
	load_args->envc = 0;
	dest = load_args->buffer;
	for (i=0; i<argc; i++) {
		src = argv[i];
		while ((*dest++ = *src++) != 0);
	}


	/* create new task */
	return create_task(id, user_entry, load_args, NORMAL_PRIO);
}

/** @brief Wakeup a blocked task
 * @param id The task's tid_t structure
 * @return
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int wakeup_task(tid_t id)
{
	task_t* task;
	uint32_t prio;
	int ret = -EINVAL;
	uint8_t flags;

	flags = irq_nested_disable();

	task = task_table + id;
	prio = task->prio;

	if (task->status == TASK_BLOCKED) {
		task->status = TASK_READY;
		ret = 0;

		spinlock_irqsave_lock(&readyqueues.lock);
		// increase the number of ready tasks
		readyqueues.nr_tasks++;

		// add task to the runqueue
		if (!readyqueues.queue[prio-1].last) {
			readyqueues.queue[prio-1].last = readyqueues.queue[prio-1].first = task;
			task->next = task->prev = NULL;
			readyqueues.prio_bitmap |= (1 << prio);
		} else {
			task->prev = readyqueues.queue[prio-1].last;
			task->next = NULL;
			readyqueues.queue[prio-1].last->next = task;
			readyqueues.queue[prio-1].last = task;
		}
		spinlock_irqsave_unlock(&readyqueues.lock);
	}

	irq_nested_enable(flags);

	return ret;
}

/** @brief Block current task
 *
 * The current task's status will be changed to TASK_BLOCKED
 *
 * @return
 * - 0 on success
 * - -EINVAL (-22) on failure
 */
int block_current_task(void)
{
	tid_t id;
	uint32_t prio;
	int ret = -EINVAL;
	uint8_t flags;

	flags = irq_nested_disable();

	id = current_task->id;
	prio = current_task->prio;

	if (task_table[id].status == TASK_RUNNING) {
		task_table[id].status = TASK_BLOCKED;
		ret = 0;

		spinlock_irqsave_lock(&readyqueues.lock);
		// reduce the number of ready tasks
		readyqueues.nr_tasks--;

		// remove task from queue
		if (task_table[id].prev)
			task_table[id].prev->next = task_table[id].next;
		if (task_table[id].next)
			task_table[id].next->prev = task_table[id].prev;
		if (readyqueues.queue[prio-1].first == task_table+id)
			readyqueues.queue[prio-1].first = task_table[id].next;
		if (readyqueues.queue[prio-1].last == task_table+id) {
			readyqueues.queue[prio-1].last = task_table[id].prev;
			if (!readyqueues.queue[prio-1].last)
				readyqueues.queue[prio-1].last = readyqueues.queue[prio-1].first;
		}

		// No valid task in queue => update prio_bitmap
		if (!readyqueues.queue[prio-1].first)
			readyqueues.prio_bitmap &= ~(1 << prio);
		spinlock_irqsave_unlock(&readyqueues.lock);
	}

	irq_nested_enable(flags);

	return ret;
}

size_t** scheduler(void)
{
	task_t* orig_task;
	uint32_t prio;

	orig_task = current_task;

	spinlock_irqsave_lock(&readyqueues.lock);

	/* signalizes that this task could be reused */
	if (current_task->status == TASK_FINISHED) {
		current_task->status = TASK_INVALID;
		readyqueues.old_task = current_task;
	} else readyqueues.old_task = NULL; // reset old task

	prio = msb(readyqueues.prio_bitmap); // determines highest priority
	if (prio > MAX_PRIO) {
		if ((current_task->status == TASK_RUNNING) || (current_task->status == TASK_IDLE))
			goto get_task_out;
		current_task = readyqueues.idle;
	} else {
		// Does the current task have an higher priority? => no task switch
		if ((current_task->prio > prio) && (current_task->status == TASK_RUNNING))
			goto get_task_out;

		if (current_task->status == TASK_RUNNING) {
			current_task->status = TASK_READY;
			readyqueues.old_task = current_task;
		}

		current_task = readyqueues.queue[prio-1].first;
		if (BUILTIN_EXPECT(current_task->status == TASK_INVALID, 0)) {
			kprintf("Upps!!!!!!! Got invalid task %d, orig task %d\n", current_task->id, orig_task->id);
		}
		current_task->status = TASK_RUNNING;

		// remove new task from queue
		// by the way, priority 0 is only used by the idle task and doesn't need own queue
		readyqueues.queue[prio-1].first = current_task->next;
		if (!current_task->next) {
			readyqueues.queue[prio-1].last = NULL;
			readyqueues.prio_bitmap &= ~(1 << prio);
		}
		current_task->next = current_task->prev = NULL;
	}

get_task_out:
	spinlock_irqsave_unlock(&readyqueues.lock);

	if (current_task != orig_task) {
		//kprintf("schedule from %u to %u with prio %u\n", orig_task->id, current_task->id, (uint32_t)current_task->prio);

		return (size_t**) &(orig_task->last_stack_pointer);
	}

	return NULL;
}

void reschedule(void)
{
	size_t** stack;
	uint8_t flags;

	flags = irq_nested_disable();
	if ((stack = scheduler()))
		switch_context(stack);
	irq_nested_enable(flags);
}
