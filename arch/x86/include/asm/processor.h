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
 * @file arch/x86/include/asm/processor.h
 * @brief CPU-specific functions
 *
 * This file contains structures and functions related to CPU-specific assembler commands.
 */

#ifndef __ARCH_PROCESSOR_H__
#define __ARCH_PROCESSOR_H__

#include <eduos/stddef.h>
#include <asm/gdt.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Read out time stamp counter
 *
 * The rdtsc asm command puts a 64 bit time stamp value
 * into EDX:EAX.
 *
 * @return The 64 bit time stamp value
 */
inline static uint64_t rdtsc(void)
{
	uint64_t x;
	asm volatile ("rdtsc" : "=A" (x));
	return x;
}

/** @brief Read cr3 register
 * @return cr3's value
 */
static inline size_t read_cr3(void) {
	size_t val;
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

/** @brief Read cr2 register
 * @return cr2's value
 */
static inline size_t read_cr2(void) {
	size_t val;
	asm volatile("mov %%cr2, %0" : "=r"(val));
	return val;
}

/** @brief Write a value into cr2 register
 * @param val The value you want to write into cr2
 */
static inline void write_cr2(size_t val) {
	asm volatile("mov %0, %%cr2" : : "r"(val));
}

/** @brief Write a value into cr3 register
 * @param val The value you want to write into cr3
 */
static inline void write_cr3(size_t val) {
	asm volatile("mov %0, %%cr3" : : "r"(val));
}

/** @brief Flush cache
 *
 * The wbinvd asm instruction which stands for "Write back and invalidate"
 * is used here
 */
inline static void flush_cache(void) {
	asm volatile ("wbinvd" : : : "memory");
}

/** @brief Flush Translation Lookaside Buffer
 *
 * Just reads cr3 and writes the same value back into it.
 */
static inline void flush_tlb(void)
{
	size_t val = read_cr3();

	if (val)
		write_cr3(val);
}

/** @brief Flush a specific page entry in TLB
 * @param addr The (virtual) address of the page to flush
 */
static inline void tlb_flush_one_page(size_t addr)
{
	asm volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

/** @brief Invalidate cache
 *
 * The invd asm instruction which invalidates cache without writing back
 * is used here
 */
inline static void invalid_cache(void) {
	asm volatile ("invd");
}

/// Force strict CPU ordering, serializes load and store operations.
inline static void mb(void) { asm volatile("mfence" ::: "memory"); }
/// Force strict CPU ordering, serializes load operations.
inline static void rmb(void) { asm volatile("lfence" ::: "memory"); }
/// Force strict CPU ordering, serializes store operations.
inline static void wmb(void) { asm volatile("sfence" ::: "memory"); }

/** @brief Read EFLAGS
 *
 * @return The EFLAGS value
 */
static inline uint32_t read_eflags(void)
{
	uint32_t result;
	asm volatile ("pushf; pop %0" : "=r"(result));
	return result;
}

/** @brief search the first most significant bit
 *
 * @param i source operand
 * @return 
 * - first bit, which is set in the source operand
 * - invalid value, if not bit ist set
 */
static inline size_t msb(size_t i)
{
	size_t ret;

	if (!i)
		return (sizeof(size_t)*8);
	asm volatile ("bsr %1, %0" : "=r"(ret) : "r"(i) : "cc");

	return ret;
}

/** @brief search the least significant bit
 *
 * @param i source operand
 * @return 
 * - first bit, which is set in the source operand
 * - invalid value, if not bit ist set
 */
static inline size_t lsb(size_t i)
{
	size_t ret;

	if (!i)
		return (sizeof(size_t)*8);
	asm volatile ("bsf %1, %0" : "=r"(ret) : "r"(i) : "cc");

	return ret;
}

/// A one-instruction-do-nothing
#define NOP1	asm  volatile ("nop")
/// A two-instruction-do-nothing
#define NOP2	asm  volatile ("nop;nop")
/// A four-instruction-do-nothing
#define NOP4	asm  volatile ("nop;nop;nop;nop")
/// A eight-instruction-do-nothing
#define NOP8	asm  volatile ("nop;nop;nop;nop;nop;nop;nop;nop")
/// The PAUSE instruction provides a hint to the processor that the code sequence is a spin-wait loop.
#define PAUSE	asm volatile ("pause")
/// The HALT instruction stops the processor until the next interrupt arrives 
#define HALT	asm volatile ("hlt")

/** @brief Init several subsystems
 *
 * This function calls the initialization procedures for:
 * - GDT
 * - APIC
 * - PCI [if configured]
 *
 * @return 0 in any case
 */
inline static int system_init(void)
{
	gdt_install();

	return 0;
}

/** @brief Detect and read out CPU frequency
 *
 * @return The CPU frequency in MHz
 */
uint32_t detect_cpu_frequency(void);

/** @brief Read out CPU frequency if detected before
 *
 * If you did not issue the detect_cpu_frequency() function before,
 * this function will call it implicitly.
 *
 * @return The CPU frequency in MHz
 */
uint32_t get_cpu_frequency(void);

/** @brief Busywait an microseconds interval of time
 * @param usecs The time to wait in microseconds
 */
void udelay(uint32_t usecs);

/** @brief System calibration
 *
 * This procedure will detect the CPU frequency and calibrate the APIC timer.
 *
 * @return 0 in any case.
 */
inline static int system_calibration(void)
{
	detect_cpu_frequency();

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif
