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
 * @author Stefan Lankes
 * @file include/eduos/tasks_types.h
 * @brief Task related structure definitions
 *
 * This file contains the task_t structure definition 
 * and task state define constants
 */

#ifndef __TASKS_TYPES_H__
#define __TASKS_TYPES_H__

#include <eduos/stddef.h>
#include <asm/tasks_types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TASK_INVALID	0
#define TASK_READY	1
#define TASK_RUNNING	2
#define TASK_BLOCKED	3
#define TASK_FINISHED	4
#define TASK_IDLE	5

#define MAX_PRIO	31
#define REALTIME_PRIO	31
#define HIGH_PRIO	16
#define NORMAL_PRIO	8
#define LOW_PRIO	1
#define IDLE_PRIO	0

typedef int (*entry_point_t)(void*);

/** @brief Represents a the process control block */
typedef struct task {
	/// Task id = position in the task table
	tid_t			id;
	/// Task status (INVALID, READY, RUNNING, ...)
	uint32_t		status;
	/// copy of the stack pointer before a context switch
	size_t*			last_stack_pointer;
	/// start address of the stack 
	void*			stack;
	/// Task priority
	uint8_t			prio;
	/// next task in the queue
	struct task*		next;
	/// previous task in the queue
	struct task*		prev;
} task_t;

typedef struct {
        task_t* first;
        task_t* last;
} task_list_t;

/** @brief Represents a queue for all runable tasks */
typedef struct {
	/// idle task
	task_t*		idle __attribute__ ((aligned (CACHE_LINE)));
        /// previous task
	task_t*		old_task;
	/// total number of tasks in the queue
	uint32_t	nr_tasks;
	/// indicates the used priority queues
	uint32_t	prio_bitmap;
	/// a queue for each priority
	task_list_t	queue[MAX_PRIO];
} runqueue_t;

#ifdef __cplusplus
}
#endif

#endif
