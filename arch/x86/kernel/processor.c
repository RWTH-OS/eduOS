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
#include <eduos/stdio.h>
#include <eduos/string.h>
#include <eduos/time.h>
#include <eduos/processor.h>
#include <eduos/tasks.h>

static uint32_t cpu_freq = 0;

uint32_t detect_cpu_frequency(void)
{
	uint64_t start, end, diff;
	uint64_t ticks, old;

	if (BUILTIN_EXPECT(cpu_freq > 0, 0))
		return cpu_freq;

	old = get_clock_tick();

	/* wait for the next time slice */
	while((ticks = get_clock_tick()) - old == 0)
		HALT;

	rmb();
	start = rdtsc();
	/* wait a second to determine the frequency */
	while(get_clock_tick() - ticks < TIMER_FREQ)
		HALT;
	rmb();
	end = rdtsc();

	diff = end > start ? end - start : start - end;
	cpu_freq = (uint32_t) (diff / (uint64_t) 1000000);

	return cpu_freq;
}

uint32_t get_cpu_frequency(void)
{	
	if (cpu_freq > 0)
		return cpu_freq;

	return detect_cpu_frequency();
}

