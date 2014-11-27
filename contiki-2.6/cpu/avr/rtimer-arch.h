/*
 * Copyright (c) 2007, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

#ifndef __RTIMER_ARCH_H__
#define __RTIMER_ARCH_H__

#include <avr/interrupt.h>

/* Nominal ARCH_SECOND is F_CPU/prescaler, e.g. 8000000/1024 = 7812
 * Other prescaler values (1, 8, 64, 256) will give greater precision
 * with shorter maximum intervals.
 * Setting RTIMER_ARCH_PRESCALER to 0 will leave Timers alone.
 * rtimer_arch_now() will then return 0, likely hanging the cpu if used.
 * Timer1 is used if Timer3 is not available.
 *
 * Note the rtimer tick to clock tick conversion will be nominally correct only
 * when the same oscillator is used for both clocks.
 * When an external 32768 watch crystal is used for clock ticks my raven CPU
 * oscillator is 1% slow, 32768 counts on crystal = ~7738 rtimer ticks.
 * For more accuracy define F_CPU to 0x800000 and optionally phase lock CPU
 * clock to 32768 crystal. This gives RTIMER_ARCH_SECOND = 8192.
 */
#ifndef RTIMER_ARCH_PRESCALER
#define RTIMER_ARCH_PRESCALER 1024UL
#endif
#if RTIMER_ARCH_PRESCALER
#define RTIMER_ARCH_SECOND (F_CPU/RTIMER_ARCH_PRESCALER)
#else
#define RTIMER_ARCH_SECOND 0
#endif


#ifdef TCNT3
#define rtimer_arch_now() (TCNT3)
#elif RTIMER_ARCH_PRESCALER
#define rtimer_arch_now() (TCNT1)
#else
#define rtimer_arch_now() (0)
#endif

void rtimer_arch_sleep(rtimer_clock_t howlong);
//#endif /* __RTIMER_ARCH_H__ */


/*-----------------------------------*/
//   "lds r26,nrk_cur_task_TCB \n\t"
//   "lds r27,nrk_cur_task_TCB+1 \n\t" 
// next push r0
//   "in      r0, __sreg__                \n\t" \
// next r31
//   "lds r26,PROCESS_CURRENT()           \n\t" \
//   "lds r27,PROCESS_CURRENT()+1         \n\t" \
//  TODO
// push r1
//   "lds     r26,nrk_kernel_stk_ptr      \n\t" \
//   "lds     r27,nrk_kernel_stk_ptr+1    \n\t" 
//  TODO END
#define SAVE_CONTEXT()  \
asm volatile (  \
   "push    r0                          \n\t" \
   "in      r0, __SREG__                \n\t" \
   "push    r0                          \n\t" \
   "push    r1                          \n\t" \
   "push    r2                          \n\t" \
   "push    r3                          \n\t" \
   "push    r4                          \n\t" \
   "push    r5                          \n\t" \
   "push    r6                          \n\t" \
   "push    r7                          \n\t" \
   "push    r8                          \n\t" \
   "push    r9                          \n\t" \
   "push    r10                         \n\t" \
   "push    r11                         \n\t" \
   "push    r12                         \n\t" \
   "push    r13                         \n\t" \
   "push    r14                         \n\t" \
   "push    r15                         \n\t" \
   "push    r16                         \n\t" \
   "push    r17                         \n\t" \
   "push    r18                         \n\t" \
   "push    r19                         \n\t" \
   "push    r20                         \n\t" \
   "push    r21                         \n\t" \
   "push    r22                         \n\t" \
   "push    r23                         \n\t" \
   "push    r24                         \n\t" \
   "push    r25                         \n\t" \
   "push    r26                         \n\t" \
   "push    r27                         \n\t" \
   "push    r28                         \n\t" \
   "push    r29                         \n\t" \
   "push    r30                         \n\t" \
   "push    r31                         \n\t" \
   "lds     r26,process_current         \n\t" \
   "lds     r27,process_current+1       \n\t" \
   "in      r0,__SP_L__                 \n\t" \
   "st x+,  r0                          \n\t" \
   "in      r0,__SP_H__                 \n\t" \
   "st x+,  r0                          \n\t" \
   "push    r1                          \n\t" \
   "ld      r1,-x                       \n\t" \
   "out __SP_H__, r27                   \n\t" \
   "out __SP_L__, r26                   \n\t" \
   "ret                                 \n\t" \
);


//   "lds r27,nrk_high_ready_TCB \n\t"
//   "lds r27,nrk_high_ready_TCB+1 \n\t" 
// top
//   "lds     r27,PROCESS_EXECUTED_NEXT() \n\t" \
//   "lds     r27,PROCESS_EXECUTED_NEXT()+1 \n\t" \
//;x points to &OSTCB[x]
#define RESTORE_CONTEXT()   \
asm volatile (  \
   "ld      r28,x+                      \n\t" \
   "out __SP_L__, r28                   \n\t" \
   "ld      r29,x+                      \n\t" \
   "out __SP_H__, r29                   \n\t" \
   "pop     r31                         \n\t" \
   "pop     r30                         \n\t" \
   "pop     r29                         \n\t" \
   "pop     r28                         \n\t" \
   "pop     r27                         \n\t" \
   "pop     r26                         \n\t" \
   "pop     r25                         \n\t" \
   "pop     r24                         \n\t" \
   "pop     r23                         \n\t" \
   "pop     r22                         \n\t" \
   "pop     r21                         \n\t" \
   "pop     r20                         \n\t" \
   "pop     r19                         \n\t" \
   "pop     r18                         \n\t" \
   "pop     r17                         \n\t" \
   "pop     r16                         \n\t" \
   "pop     r15                         \n\t" \
   "pop     r14                         \n\t" \
   "pop     r13                         \n\t" \
   "pop     r12                         \n\t" \
   "pop     r11                         \n\t" \
   "pop     r10                         \n\t" \
   "pop     r9                          \n\t" \
   "pop     r8                          \n\t" \
   "pop     r7                          \n\t" \
   "pop     r6                          \n\t" \
   "pop     r5                          \n\t" \
   "pop     r4                          \n\t" \
   "pop     r3                          \n\t" \
   "pop     r2                          \n\t" \
   "pop     r1                          \n\t" \
   "pop     r0                          \n\t" \
   "out __SREG__, r0                    \n\t" \
   "pop     r0                          \n\t" \
   "reti                                \n\t" \
);


//#define SAVE_CONTEXT()  \
//asm volatile (  \
//   "push    r0                          \n\t" \
//   "push    r1                          \n\t" \
//   "push    r2                          \n\t" \
//   "push    r3                          \n\t" \
//   "push    r4                          \n\t" \
//   "push    r5                          \n\t" \
//   "push    r6                          \n\t" \
//   "push    r7                          \n\t" \
//   "push    r8                          \n\t" \
//   "push    r9                          \n\t" \
//   "push    r10                         \n\t" \
//   "push    r11                         \n\t" \
//   "push    r12                         \n\t" \
//   "push    r13                         \n\t" \
//   "push    r14                         \n\t" \
//   "push    r15                         \n\t" \
//   "push    r16                         \n\t" \
//   "push    r17                         \n\t" \
//   "push    r18                         \n\t" \
//   "push    r19                         \n\t" \
//   "push    r20                         \n\t" \
//   "push    r21                         \n\t" \
//   "push    r22                         \n\t" \
//   "push    r23                         \n\t" \
//   "push    r24                         \n\t" \
//   "push    r25                         \n\t" \
//   "push    r26                         \n\t" \
//   "push    r27                         \n\t" \
//   "push    r28                         \n\t" \
//   "push    r29                         \n\t" \
//   "push    r30                         \n\t" \
//   "push    r31                         \n\t" \
//);

//#define RESTORE_CONTEXT()   \
//asm volatile (  \
//   "pop     r31                         \n\t" \
//   "pop     r30                         \n\t" \
//   "pop     r29                         \n\t" \
//   "pop     r28                         \n\t" \
//   "pop     r27                         \n\t" \
//   "pop     r26                         \n\t" \
//   "pop     r25                         \n\t" \
//   "pop     r24                         \n\t" \
//   "pop     r23                         \n\t" \
//   "pop     r22                         \n\t" \
//   "pop     r21                         \n\t" \
//   "pop     r20                         \n\t" \
//   "pop     r19                         \n\t" \
//   "pop     r18                         \n\t" \
//   "pop     r17                         \n\t" \
//   "pop     r16                         \n\t" \
//   "pop     r15                         \n\t" \
//   "pop     r14                         \n\t" \
//   "pop     r13                         \n\t" \
//   "pop     r12                         \n\t" \
//   "pop     r11                         \n\t" \
//   "pop     r10                         \n\t" \
//   "pop     r9                          \n\t" \
//   "pop     r8                          \n\t" \
//   "pop     r7                          \n\t" \
//   "pop     r6                          \n\t" \
//   "pop     r5                          \n\t" \
//   "pop     r4                          \n\t" \
//   "pop     r3                          \n\t" \
//   "pop     r2                          \n\t" \
//   "pop     r1                          \n\t" \
//   "pop     r0                          \n\t" \
//);


#endif /* __RTIMER_ARCH_H__ */
