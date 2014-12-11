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
 * @file include/eduos/semaphore.h
 * @brief semaphore functions definition
 */

#ifndef __SEMAPHORE_H__
#define __SEMAPHORE_H__

#include <eduos/string.h>
#include <eduos/tasks.h>
#include <eduos/semaphore_types.h>
#include <eduos/spinlock.h>
#include <eduos/errno.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Semaphore initialization
 *
 * Always init semaphores before use!
 *
 * @param s Pointer to semaphore structure to initialize
 * @param v Resource count
 *
 * @return
 * - 0 on success
 * - -EINVAL on invalid argument
 */
inline static int sem_init(sem_t* s, unsigned int v) {
	unsigned int i;

	if (BUILTIN_EXPECT(!s, 0))
		return -EINVAL;

	s->value = v;
	s->pos = 0;
	for(i=0; i<MAX_TASKS; i++)
		s->queue[i] = MAX_TASKS;
	spinlock_irqsave_init(&s->lock);

	return 0;
}

/** @brief Destroy semaphore
 * @return
 * - 0 on success
 * - -EINVAL on invalid argument
 */
inline static int sem_destroy(sem_t* s) {
	if (BUILTIN_EXPECT(!s, 0))
		return -EINVAL;

	spinlock_irqsave_destroy(&s->lock);

	return 0;
}

/** @brief Nonblocking trywait for sempahore
 *
 * Will return immediately if not available
 *
 * @return
 * - 0 on success (You got the semaphore)
 * - -EINVAL on invalid argument
 * - -ECANCELED on failure (You still have to wait)
 */
inline static int sem_trywait(sem_t* s) {
	int ret = -ECANCELED;

	if (BUILTIN_EXPECT(!s, 0))
		return -EINVAL;

	spinlock_irqsave_lock(&s->lock);
	if (s->value > 0) {
		s->value--;
		ret = 0;
	}
	spinlock_irqsave_unlock(&s->lock);

	return ret;
}

/** @brief Blocking wait for semaphore
 *
 * @param s Address of the according sem_t structure
 * @return 
 * - 0 on success
 * - -EINVAL on invalid argument
 * - -ETIME on timer expired
 */
inline static int sem_wait(sem_t* s) {
	if (BUILTIN_EXPECT(!s, 0))
		return -EINVAL;

next_try1:
	spinlock_irqsave_lock(&s->lock);
	if (s->value > 0) {
		s->value--;
		spinlock_irqsave_unlock(&s->lock);
	} else {
		s->queue[s->pos] = current_task->id;
		s->pos = (s->pos + 1) % MAX_TASKS;
		block_current_task();
		spinlock_irqsave_unlock(&s->lock);
		reschedule();
		goto next_try1;
	}

	return 0;
}

/** @brief Give back resource 
 * @return
 * - 0 on success
 * - -EINVAL on invalid argument
 */
inline static int sem_post(sem_t* s) {
	if (BUILTIN_EXPECT(!s, 0))
		return -EINVAL;

	spinlock_irqsave_lock(&s->lock);
	if (s->value > 0) {
		s->value++;
		spinlock_irqsave_unlock(&s->lock);
	} else {
		unsigned int k, i;

		s->value++;
		i = s->pos;
		for(k=0; k<MAX_TASKS; k++) {
			if (s->queue[i] < MAX_TASKS) {
				wakeup_task(s->queue[i]);
				s->queue[i] = MAX_TASKS;
				break;
			}
			i = (i + 1) % MAX_TASKS;
		}
		spinlock_irqsave_unlock(&s->lock);
	}

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
