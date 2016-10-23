/**
 * @author Jan Martens
 * @file arch/x86/include/asm/timer.h
 * @brief simple Sleep Function
 *
 * This file contains structures and functions related to a simple Sleep Function.
 */

 #ifndef __ARCH_TIMER_H__
 #define __ARCH_TIMER_H__

 #include <eduos/stddef.h>
 #include <eduos/stdio.h>
 #include <eduos/string.h>
 #include <eduos/processor.h>

 #ifdef __cplusplus
 extern "C" {
 #endif

 /** @brief Sleep Function */
 void sleep(unsigned int seconds);

 #ifdef __cplusplus
 }
 #endif
 #endif
