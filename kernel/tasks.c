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
#include <eduos/tasks.h>
#include <eduos/tasks_types.h>
#include <eduos/errno.h>

extern void* default_stack_pointer;

/** @brief Array of task structures (aka PCB)
 *
 * A task's id will be its position in this array.
 */
static task_t task_table[MAX_TASKS] = { \
		[0]                 = {0, TASK_IDLE, NULL, NULL, 0, NULL, NULL}, \
		[1 ... MAX_TASKS-1] = {0, TASK_INVALID, NULL, NULL, 0, NULL, NULL}};

static runqueue_t runqueue = { task_table+0, NULL, 0, 0, {[0 ... MAX_PRIO-1] = {NULL, NULL}}};

task_t* current_task = task_table+0;

/** @brief helper function for the assembly code to determine the current task
 * @return Pointer to the task_t structure of current task
 */
task_t* get_current_task(void) 
{
	return current_task;
}

/** @brief determines the highest priority of the all runable task 
 * @return Highest priority
 */
uint32_t get_highest_priority(void)
{
	return msb(runqueue.prio_bitmap);
}

int multitasking_init(void)
{
	if (BUILTIN_EXPECT(task_table[0].status != TASK_IDLE, 0)) {
		kputs("Task 0 is not an idle task\n");
		return -ENOMEM;
	}

	task_table[0].prio = IDLE_PRIO;
	task_table[0].stack = default_stack_pointer - 8192;

	return 0;
}

void finish_task_switch(void)
{
	task_t* old;
	uint8_t prio;

	if ((old = runqueue.old_task) != NULL) {
		if (old->status == TASK_INVALID) {
			old->stack = NULL;
			old->last_stack_pointer = NULL;
			runqueue.old_task = NULL;
		} else {
			prio = old->prio;
			if (!runqueue.queue[prio-1].first) {
				old->next = old->prev = NULL;
				runqueue.queue[prio-1].first = runqueue.queue[prio-1].last = old;
			} else {
				old->next = NULL;
				old->prev = runqueue.queue[prio-1].last;
				runqueue.queue[prio-1].last->next = old;
				runqueue.queue[prio-1].last = old;
			}
			runqueue.old_task = NULL;
			runqueue.prio_bitmap |= (1 << prio);
		}
	}
}

/** @brief A procedure to be called by
 * procedures which are called by exiting tasks. */
static void NORETURN do_exit(int arg)
{
	task_t* curr_task = current_task;

	kprintf("Terminate task: %u, return value %d\n", curr_task->id, arg);

	curr_task->status = TASK_FINISHED;
	reschedule();

	kprintf("Kernel panic: scheduler found no valid task\n");
	while(1) {
		NOP8;
	}
}

/** @brief A procedure to be called by kernel tasks */
void NORETURN leave_kernel_task(void) {
	int result;

	result = 0; //get_return_value();
	do_exit(result);
}

/** @brief Create a task with a specific entry point
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

	for(i=0; i<MAX_TASKS; i++) {
		if (task_table[i].status == TASK_INVALID) {
			task_table[i].id = i;
			task_table[i].status = TASK_READY;
			task_table[i].last_stack_pointer = NULL;
			task_table[i].stack = create_stack(i);
			task_table[i].prio = prio;

			if (id)
				*id = i;

			ret = create_default_frame(task_table+i, ep, arg);

			// add task in the runqueue
			runqueue.prio_bitmap |= (1 << prio);
			runqueue.nr_tasks++;
			if (!runqueue.queue[prio-1].first) {
				task_table[i].next = task_table[i].prev = NULL;
				runqueue.queue[prio-1].first = task_table+i;
				runqueue.queue[prio-1].last = task_table+i;
			} else {
				task_table[i].prev = runqueue.queue[prio-1].last;
				task_table[i].next = NULL;
				runqueue.queue[prio-1].last->next = task_table+i;
				runqueue.queue[prio-1].last = task_table+i;
			}
			break;
		}
	}

	return ret;
}

int create_kernel_task(tid_t* id, entry_point_t ep, void* args, uint8_t prio)
{
	if (prio > MAX_PRIO)
		prio = NORMAL_PRIO;

	return create_task(id, ep, args, prio);
}

size_t** scheduler(void)
{
	task_t* orig_task;
	uint32_t prio;

	orig_task = current_task;

	/* signalizes that this task could be reused */
	if (current_task->status == TASK_FINISHED) {
		current_task->status = TASK_INVALID;
		runqueue.old_task = current_task;
	} else runqueue.old_task = NULL; // reset old task

	prio = msb(runqueue.prio_bitmap); // determines highest priority
	if (prio > MAX_PRIO) {
		if ((current_task->status == TASK_RUNNING) || (current_task->status == TASK_IDLE))
			goto get_task_out;
		current_task = runqueue.idle;
	} else {
		// Does the current task have an higher priority? => no task switch
		if ((current_task->prio > prio) && (current_task->status == TASK_RUNNING))
			goto get_task_out;

		if (current_task->status == TASK_RUNNING) {
			current_task->status = TASK_READY;
			runqueue.old_task = current_task;
		}

		current_task = runqueue.queue[prio-1].first;
		if (BUILTIN_EXPECT(current_task->status == TASK_INVALID, 0)) {
			kprintf("Upps!!!!!!! Got invalid task %d, orig task %d\n", current_task->id, orig_task->id);
		}
		current_task->status = TASK_RUNNING;

		// remove new task from queue
		runqueue.queue[prio-1].first = current_task->next;
		if (!current_task->next) {
			runqueue.queue[prio-1].last = NULL;
			runqueue.prio_bitmap &= ~(1 << prio);
		}
		current_task->next = current_task->prev = NULL;
	}

get_task_out:
	if (current_task != orig_task) {
		//kprintf("schedule from %u to %u with prio %u\n", orig_task->id, curr_task->id, (uint32_t)current_task->prio);

		return (size_t**) &(orig_task->last_stack_pointer);
	}

	return NULL;
}

void reschedule(void)
{
	size_t** stack;
	if ((stack = scheduler()))
		switch_context(stack);
}
