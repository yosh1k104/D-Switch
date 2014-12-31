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
 * $Id: rtimer-arch.c,v 1.10 2010/02/28 21:29:19 dak664 Exp $
 */

/**
 * \file
 *         AVR-specific rtimer code
 *         Defaults to Timer3 for those ATMEGAs that have it.
 *         If Timer3 not present Timer1 will be used.
 * \author
 *         Fredrik Osterlind <fros@sics.se>
 *         Joakim Eriksson <joakime@sics.se>
 */

/* OBS: 8 seconds maximum time! */

#include <avr/io.h>
#include <avr/interrupt.h> /* PATH="/usr/local/avr/include/avr" */
#include <stdio.h>

#include "sys/energest.h"
#include "sys/rtimer.h"
#include "sys/process.h"
#include "rtimer-arch.h"




#define PROCESS_STATE_NONE        0
#define PROCESS_STATE_RUNNING     1
#define PROCESS_STATE_CALLED      2
#define PROCESS_STATE_WAITING     3
#define PROCESS_STATE_READY       4
#define PROCESS_STATE_SUSPENDED   5
#define PROCESS_STATE_FINISHED    6



#if defined(__AVR_ATmega1281__) || defined(__AVR_ATmega1284P__)
#define ETIMSK TIMSK3
#define ETIFR TIFR3
#define TICIE3 ICIE3

//Has no 'C', so we just set it to B. The code doesn't really use C so this
//is safe to do but lets it compile. Probably should enable the warning if
//it is ever used on other platforms.
//#warning no OCIE3C in timer3 architecture, hopefully it won't be needed!

#define OCIE3C	OCIE3B
#define OCF3C	OCF3B
#endif

#if defined(__AVR_AT90USB1287__) || defined(__AVR_ATmega128RFA1__) 
#define ETIMSK TIMSK3
#define ETIFR TIFR3
#define TICIE3 ICIE3
#endif

#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega644__)
#define TIMSK TIMSK1
#define TICIE1 ICIE1
#define TIFR TIFR1
#endif

/* Track flow through rtimer interrupts*/
#if DEBUGFLOWSIZE&&0
extern uint8_t debugflowsize,debugflow[DEBUGFLOWSIZE];
#define DEBUGFLOW(c) if (debugflowsize<(DEBUGFLOWSIZE-1)) debugflow[debugflowsize++]=c
#else
#define DEBUGFLOW(c)
#endif

/* this value shows "1" when called process is none */
static int suspend_flag = 0;

volatile static int chcontext_count = 0; // +
int num_stacks = 0;

volatile uint8_t *process_stack_ptr = 0;

/*---------------------------------------------------------------------------*/
#define SAVE_CONTEXT()  \
asm volatile (  \
   "push    r0                              \n\t" \
   "in      r0, __SREG__                    \n\t" \
   "cli                                     \n\t" \
   "push    r0                              \n\t" \
   "push    r1                              \n\t" \
   "clr     r1                              \n\t" \
   "push    r2                              \n\t" \
   "push    r3                              \n\t" \
   "push    r4                              \n\t" \
   "push    r5                              \n\t" \
   "push    r6                              \n\t" \
   "push    r7                              \n\t" \
   "push    r8                              \n\t" \
   "push    r9                              \n\t" \
   "push    r10                             \n\t" \
   "push    r11                             \n\t" \
   "push    r12                             \n\t" \
   "push    r13                             \n\t" \
   "push    r14                             \n\t" \
   "push    r15                             \n\t" \
   "push    r16                             \n\t" \
   "push    r17                             \n\t" \
   "push    r18                             \n\t" \
   "push    r19                             \n\t" \
   "push    r20                             \n\t" \
   "push    r21                             \n\t" \
   "push    r22                             \n\t" \
   "push    r23                             \n\t" \
   "push    r24                             \n\t" \
   "push    r25                             \n\t" \
   "push    r26                             \n\t" \
   "push    r27                             \n\t" \
   "push    r28                             \n\t" \
   "push    r29                             \n\t" \
   "push    r30                             \n\t" \
   "push    r31                             \n\t" \
   "lds     r26,process_stack_ptr           \n\t" \
   "lds     r27,process_stack_ptr+1         \n\t" \
   "in      r0,__SP_L__                     \n\t" \
   "st x+,  r0                              \n\t" \
   "in      r0,__SP_H__                     \n\t" \
   "st x+,  r0                              \n\t" \
);
/*---------------------------------------------------------------------------*/
#define RESTORE_CONTEXT()   \
asm volatile (  \
   "lds     r26,process_stack_ptr           \n\t" \
   "lds     r27,process_stack_ptr+1         \n\t" \
   "ld      r28,x+                          \n\t" \
   "out __SP_L__, r28                       \n\t" \
   "ld      r29,x+                          \n\t" \
   "out __SP_H__, r29                       \n\t" \
   "pop     r31                             \n\t" \
   "pop     r30                             \n\t" \
   "pop     r29                             \n\t" \
   "pop     r28                             \n\t" \
   "pop     r27                             \n\t" \
   "pop     r26                             \n\t" \
   "pop     r25                             \n\t" \
   "pop     r24                             \n\t" \
   "pop     r23                             \n\t" \
   "pop     r22                             \n\t" \
   "pop     r21                             \n\t" \
   "pop     r20                             \n\t" \
   "pop     r19                             \n\t" \
   "pop     r18                             \n\t" \
   "pop     r17                             \n\t" \
   "pop     r16                             \n\t" \
   "pop     r15                             \n\t" \
   "pop     r14                             \n\t" \
   "pop     r13                             \n\t" \
   "pop     r12                             \n\t" \
   "pop     r11                             \n\t" \
   "pop     r10                             \n\t" \
   "pop     r9                              \n\t" \
   "pop     r8                              \n\t" \
   "pop     r7                              \n\t" \
   "pop     r6                              \n\t" \
   "pop     r5                              \n\t" \
   "pop     r4                              \n\t" \
   "pop     r3                              \n\t" \
   "pop     r2                              \n\t" \
   "pop     r1                              \n\t" \
   "pop     r0                              \n\t" \
   "out __SREG__, r0                        \n\t" \
   "pop     r0                              \n\t" \
);
/*---------------------------------------------------------------------------*/
//uint8_t 
//move_stack_ptr(uint8_t *stack_ptr)
//{
//  /* simulate stack after a call to savecontext */
//  *stack_ptr = 0x00;  //necessary for reti to line up
//  stack_ptr--;
//  *stack_ptr = 0x00;  //r0
//  stack_ptr--;
//  *stack_ptr = 0x00;  //r1 wants to always be 0
//  stack_ptr--;
//  *stack_ptr = 0x02;  //r2
//  stack_ptr--;
//  *stack_ptr = 0x03;  //r3
//  stack_ptr--;
//  *stack_ptr = 0x04;  //r4
//  stack_ptr--;
//  *stack_ptr = 0x05;  //r5
//  stack_ptr--;
//  *stack_ptr = 0x06;  //r6
//  stack_ptr--;
//  *stack_ptr = 0x07;  //r7
//  stack_ptr--;
//  *stack_ptr = 0x08;  //r8
//  stack_ptr--;
//  *stack_ptr = 0x09;  //r9
//  stack_ptr--;
//  *stack_ptr = 0x10;  //r10
//  stack_ptr--;
//  *stack_ptr = 0x11;  //r11
//  stack_ptr--;
//  *stack_ptr = 0x12;  //r12
//  stack_ptr--;
//  *stack_ptr = 0x13;  //r13
//  stack_ptr--;
//  *stack_ptr = 0x14;  //r14
//  stack_ptr--;
//  *stack_ptr = 0x15;  //r15
//  stack_ptr--;
//  *stack_ptr = 0x16;  //r16
//  stack_ptr--;
//  *stack_ptr = 0x17;  //r17
//  stack_ptr--;
//  *stack_ptr = 0x18;  //r18
//  stack_ptr--;
//  *stack_ptr = 0x19;  //r19
//  stack_ptr--;
//  *stack_ptr = 0x20;  //r20
//  stack_ptr--;
//  *stack_ptr = 0x21;  //r21
//  stack_ptr--;
//  *stack_ptr = 0x22;  //r22
//  stack_ptr--;
//  *stack_ptr = 0x23;  //r23
//  stack_ptr--;
//  *stack_ptr = 0x24;  //r24
//  stack_ptr--;
//  *stack_ptr = 0x25;  //r25
//  stack_ptr--;
//  *stack_ptr = 0x26;  //r26
//  stack_ptr--;
//  *stack_ptr = 0x27;  //r27
//  stack_ptr--;
//  *stack_ptr = 0x28;  //r28
//  stack_ptr--;
//  *stack_ptr = 0x29;  //r29
//  stack_ptr--;
//  *stack_ptr = 0x30;  //r30
//  stack_ptr--;
//  *stack_ptr = 0x31;  //r31
//  stack_ptr--;
//
//  return 1;
//}
/*---------------------------------------------------------------------------*/
uint8_t
init_process_stack(struct process *p, void *task_ptr, uint8_t *buffer_ptr, uint16_t stack_size)
{
  printf("\nStart of init_process_stack\n\n");

  unsigned int this_address;

  /* stack grows down in memory */
  uint8_t *stack_ptr = buffer_ptr + stack_size - 1;


  /*
   *  processing when error occurred
   */
  //if (num_stacks > MAX_NUM_STACKS) {
  //  printf("num_stacks: %d\n", num_stacks);
  //  printf("MAX_NUM_STACKS: %d\n", MAX_NUM_STACKS);
  //  /* error, tcb is not big enough, need to increase max_numtasks in uik.h */
  //  return -1;
  //}


  /*  setup parameters
   *  "initialized" could be changed to ready without much effect, but UIKRun would
   *  not have to be called
   */
  //tcb[numtasks].state = initialized;
  //tcb[numtasks].delay = 0;




  /* place a few known bytes on the bottom - useful for debugging */
  /* tcb[numtasks].stack_lptr = stack_ptr; */
  // TODO
  //tcb[numtasks].stack_ptr = stack_ptr - 1;
  p->stack_ptr = stack_ptr - 1;

  /* will be location of most significant part of stack address */
  *stack_ptr = 0x11;
  stack_ptr--;
  /* least significant byte of stack address */
  *stack_ptr = 0x22;
  stack_ptr--;
  *stack_ptr = 0x33;
  stack_ptr--;

  /* address of the executing function */
  this_address = task_ptr;
  *stack_ptr   = this_address & 0x00ff;
  stack_ptr--;
  this_address >>= 8;
  *stack_ptr = this_address & 0x00ff;
  stack_ptr--;

  /* simulate stack after a call to savecontext */
  *stack_ptr = 0x00;  //necessary for reti to line up
  stack_ptr--;
  *stack_ptr = 0x00;  //r0
  stack_ptr--;
  *stack_ptr = 0x00;  //r1 wants to always be 0
  stack_ptr--;
  *stack_ptr = 0x02;  //r2
  stack_ptr--;
  *stack_ptr = 0x03;  //r3
  stack_ptr--;
  *stack_ptr = 0x04;  //r4
  stack_ptr--;
  *stack_ptr = 0x05;  //r5
  stack_ptr--;
  *stack_ptr = 0x06;  //r6
  stack_ptr--;
  *stack_ptr = 0x07;  //r7
  stack_ptr--;
  *stack_ptr = 0x08;  //r8
  stack_ptr--;
  *stack_ptr = 0x09;  //r9
  stack_ptr--;
  *stack_ptr = 0x10;  //r10
  stack_ptr--;
  *stack_ptr = 0x11;  //r11
  stack_ptr--;
  *stack_ptr = 0x12;  //r12
  stack_ptr--;
  *stack_ptr = 0x13;  //r13
  stack_ptr--;
  *stack_ptr = 0x14;  //r14
  stack_ptr--;
  *stack_ptr = 0x15;  //r15
  stack_ptr--;
  *stack_ptr = 0x16;  //r16
  stack_ptr--;
  *stack_ptr = 0x17;  //r17
  stack_ptr--;
  *stack_ptr = 0x18;  //r18
  stack_ptr--;
  *stack_ptr = 0x19;  //r19
  stack_ptr--;
  *stack_ptr = 0x20;  //r20
  stack_ptr--;
  *stack_ptr = 0x21;  //r21
  stack_ptr--;
  *stack_ptr = 0x22;  //r22
  stack_ptr--;
  *stack_ptr = 0x23;  //r23
  stack_ptr--;
  *stack_ptr = 0x24;  //r24
  stack_ptr--;
  *stack_ptr = 0x25;  //r25
  stack_ptr--;
  *stack_ptr = 0x26;  //r26
  stack_ptr--;
  *stack_ptr = 0x27;  //r27
  stack_ptr--;
  *stack_ptr = 0x28;  //r28
  stack_ptr--;
  *stack_ptr = 0x29;  //r29
  stack_ptr--;
  *stack_ptr = 0x30;  //r30
  stack_ptr--;
  *stack_ptr = 0x31;  //r31
  stack_ptr--;

  /* store the address of the stack */
  this_address = stack_ptr;
  *(p->stack_ptr) = (this_address & 0xff);
  this_address >>= 8;
  *(p->stack_ptr + 1) = (this_address & 0xff);

  num_stacks++;

  printf("\nEnd of init_process_stack\n\n");

  /* return the task id */
  //return (num_stacks - 1);
  return num_stacks;
}
/*---------------------------------------------------------------------------*/
//uint8_t
//init_sub_stack(struct process *p, void *task_ptr, uint8_t *buffer_ptr, uint16_t stack_size)
//{
//  printf("\nStart of init_sub_stack\n\n");
//
//  unsigned int this_address;
//
//  /* stack grows down in memory */
//  uint8_t *stack_ptr = buffer_ptr + stack_size - 1;
//
//
//  if (num_stacks > MAX_NUM_STACKS) {
//    /* error, tcb is not big enough, need to increase max_numtasks in uik.h */
//    return -1;
//  }
//
//
//
//  /*  setup parameters
//   *  "initialized" could be changed to ready without much effect, but UIKRun would
//   *  not have to be called
//   */
//  //tcb[numtasks].state = initialized;
//  //tcb[numtasks].delay = 0;
//
//
//
//
//  /* place a few known bytes on the bottom - useful for debugging */
//  /* tcb[numtasks].stack_lptr = stack_ptr; */
//  // TODO
//  //tcb[numtasks].stack_ptr = stack_ptr - 1;
//  p->stack_ptr = stack_ptr - 1;
//
//  /* will be location of most significant part of stack address */
//  *stack_ptr = 0x11;
//  stack_ptr--;
//  /* least significant byte of stack address */
//  *stack_ptr = 0x22;
//  stack_ptr--;
//  *stack_ptr = 0x33;
//  stack_ptr--;
//
//  /* address of the executing function */
//  this_address = task_ptr;
//  *stack_ptr   = this_address & 0x00ff;
//  stack_ptr--;
//  this_address >>= 8;
//  *stack_ptr = this_address & 0x00ff;
//  stack_ptr--;
//
//  /* simulate stack after a call to savecontext */
//  *stack_ptr = 0x00;  //necessary for reti to line up
//  stack_ptr--;
//  *stack_ptr = 0x00;  //r0
//  stack_ptr--;
//  *stack_ptr = 0x00;  //r1 wants to always be 0
//  stack_ptr--;
//  *stack_ptr = 0x02;  //r2
//  stack_ptr--;
//  *stack_ptr = 0x03;  //r3
//  stack_ptr--;
//  *stack_ptr = 0x04;  //r4
//  stack_ptr--;
//  *stack_ptr = 0x05;  //r5
//  stack_ptr--;
//  *stack_ptr = 0x06;  //r6
//  stack_ptr--;
//  *stack_ptr = 0x07;  //r7
//  stack_ptr--;
//  *stack_ptr = 0x08;  //r8
//  stack_ptr--;
//  *stack_ptr = 0x09;  //r9
//  stack_ptr--;
//  *stack_ptr = 0x10;  //r10
//  stack_ptr--;
//  *stack_ptr = 0x11;  //r11
//  stack_ptr--;
//  *stack_ptr = 0x12;  //r12
//  stack_ptr--;
//  *stack_ptr = 0x13;  //r13
//  stack_ptr--;
//  *stack_ptr = 0x14;  //r14
//  stack_ptr--;
//  *stack_ptr = 0x15;  //r15
//  stack_ptr--;
//  *stack_ptr = 0x16;  //r16
//  stack_ptr--;
//  *stack_ptr = 0x17;  //r17
//  stack_ptr--;
//  *stack_ptr = 0x18;  //r18
//  stack_ptr--;
//  *stack_ptr = 0x19;  //r19
//  stack_ptr--;
//  *stack_ptr = 0x20;  //r20
//  stack_ptr--;
//  *stack_ptr = 0x21;  //r21
//  stack_ptr--;
//  *stack_ptr = 0x22;  //r22
//  stack_ptr--;
//  *stack_ptr = 0x23;  //r23
//  stack_ptr--;
//  *stack_ptr = 0x24;  //r24
//  stack_ptr--;
//  *stack_ptr = 0x25;  //r25
//  stack_ptr--;
//  *stack_ptr = 0x26;  //r26
//  stack_ptr--;
//  *stack_ptr = 0x27;  //r27
//  stack_ptr--;
//  *stack_ptr = 0x28;  //r28
//  stack_ptr--;
//  *stack_ptr = 0x29;  //r29
//  stack_ptr--;
//  *stack_ptr = 0x30;  //r30
//  stack_ptr--;
//  *stack_ptr = 0x31;  //r31
//  stack_ptr--;
//
//  /* store the address of the stack */
//  this_address = stack_ptr;
//  *(p->stack_ptr) = (this_address & 0xff);
//  this_address >>= 8;
//  *(p->stack_ptr + 1) = (this_address & 0xff);
//
//  num_stacks++;
//
//  printf("\nEnd of init_sub_stack\n\n");
//
//  /* return the task id */
//  //return (num_stacks - 1);
//  return num_stacks;
//}
/*---------------------------------------------------------------------------*/
int
process_suspend(struct process *p)
{
  struct process *q;

  if(process_preempted != NULL) {
    printf("\nERROR SUSPENDED PROCESS HAS ALREADY EXISTED\n\n");
    return PROCESS_ERR_SUSPEND;
  }

  if(process_current->state == PROCESS_STATE_RUNNING) {
    printf("\nCALLED PROCESS IS NONE\n\n");

    //for(q = process_list; q != NULL; q = q->next) {
    //  if(q->id = process_current->id) {
    //    process_preempted = q;
    //  }
    //} /* for end */

    //process_current = p;

    p->state = PROCESS_STATE_CALLED;


    suspend_flag = 1;

    return PROCESS_ERR_RUNNING;
  }



  printf("\nsuspended\n");
  printf("process_current: id: %d - state: %d - type: %d\n", 
         process_current->id, process_current->state, process_current->type);
  printf("process_preempted: id: %d - state: %d - type: %d\n", 
         process_preempted->id, process_preempted->state, process_preempted->type);
  //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
  printf("suspended\n\n");



  for(q = process_list; q != NULL; q = q->next) {
    if(q->state == PROCESS_STATE_SUSPENDED) {
      printf("\nERROR SUSPENDED PROCESS HAS ALREADY EXISTED\n\n");
      return PROCESS_ERR_SUSPEND;
    }

    if(q->state == PROCESS_STATE_CALLED) {
      process_preempted = q;
      q->state = PROCESS_STATE_SUSPENDED;
      init_process_stack(q, q->pt.lc, main_thread_stack, STACK_SIZE);
      process_stack_ptr = q->stack_ptr;

      process_current = p;
      p->state = PROCESS_STATE_CALLED;



      printf("\nsuspended\n");
      printf("process_current: id: %d - state: %d - type: %d\n", 
             process_current->id, process_current->state, process_current->type);
      printf("process_preempted: id: %d - state: %d - type: %d\n", 
             process_preempted->id, process_preempted->state, process_preempted->type);
      //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
      printf("suspended\n\n");



      return PROCESS_ERR_OK;
    } /* if end */
  } /* for end */

  return PROCESS_ERR_SUSPEND;
}
/*---------------------------------------------------------------------------*/
int
process_resume(struct process *p)
{
  struct process *q;

  if(suspend_flag == 1) {
    for(q = process_list; q != NULL; q = q->next) {
      if(q->state == PROCESS_STATE_CALLED) {
        q->state = PROCESS_STATE_RUNNING;
        process_stack_ptr = q->stack_ptr;
      } /* if end */
    } /* for end  */

    suspend_flag = 0;
    return PROCESS_ERR_RESUME;
  }

  // TODO call init change stack function
  if(process_preempted == NULL) {
    printf("\nERROR PREEMPTED PROCESS IS NONE\n\n");
    return PROCESS_ERR_RESUME;
  }



  printf("\nresume\n");
  printf("process_current: id: %d - state: %d - type: %d\n", 
         process_current->id, process_current->state, process_current->type);
  printf("process_preempted: id: %d - state: %d - type: %d\n", 
         process_preempted->id, process_preempted->state, process_preempted->type);
  //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
  printf("resume\n\n");



  for(q = process_list; q != NULL; q = q->next) {
    if(q->state == PROCESS_STATE_SUSPENDED) {
      //p = q;
      //suspend_count = suspend_count + 1;
      process_current = q;
      q->state = PROCESS_STATE_CALLED;
      process_stack_ptr = q->stack_ptr;

      process_preempted = NULL;
      process_preempted->id = 0;
      process_preempted->state = PROCESS_STATE_NONE;
      process_preempted->type = NORMAL_TASK;
      p->state = PROCESS_STATE_RUNNING;



      printf("\nresume\n");
      printf("process_current: id: %d - state: %d - type: %d\n", 
             process_current->id, process_current->state, process_current->type);
      printf("process_preempted: id: %d - state: %d - type: %d\n", 
             process_preempted->id, process_preempted->state, process_preempted->type);
      //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
      printf("resume\n\n");



      return PROCESS_ERR_OK;
    } /* if end */
  } /* for end  */

  //if(suspend_count != 1) {
  //    printf("\nERROR SUSPENDED PROCESS IS NONE OR TOO MANY\n\n");
  //    return PROCESS_ERR_RESUME;
  //}

  //printf("process_current1: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);

  return PROCESS_ERR_RESUME;
}
/*---------------------------------------------------------------------------*/
int
process_switch_preempted()
{
  struct process *p;

  for(p = process_list; p != NULL; p = p->next) {
    if(p->type == REALTIME_TASK) {
      if(process_suspend(p) == PROCESS_ERR_OK) {
        return PROCESS_ERR_OK;
      } /* if suspend end  */
    } /* if check type end */
  } /* for end  */

  for(p = process_list; p != NULL; p = p->next) {
    if(p->id == process_current->id) {
      process_preempted = p;

      printf("process - id: %d - state: %d - type: %d\n",
              p->id, p->state, p->type);
      init_process_stack(p, p->pt.lc, main_thread_stack, STACK_SIZE);
      process_stack_ptr = p->stack_ptr;
    } /* if process id end */
  } /* for end  */

  for(p = process_list; p != NULL; p = p->next) {
    if(p->type == REALTIME_TASK) {
      process_current = p;
    }
  } /* for end  */

  return PROCESS_ERR_SUSPEND;
}
/*---------------------------------------------------------------------------*/
int
process_switch_called()
{
  struct process *p;

  printf("process_switch_called\n");

  for(p = process_list; p != NULL; p = p->next) {
    if(p->type == REALTIME_TASK) {
      if(process_resume(p) == PROCESS_ERR_OK) {
        return PROCESS_ERR_OK;
      }
    }
  } /* for end */

  for(p = process_list; p != NULL; p = p->next) {
    if(p->id == process_preempted->id) {
      process_current = p;
      process_stack_ptr = p->stack_ptr;

      process_preempted = NULL;
      process_preempted->id = 0;
      process_preempted->state = PROCESS_STATE_NONE;
      process_preempted->type = NORMAL_TASK;
    }
  } /* for end */

  return PROCESS_ERR_RESUME;
}
/*---------------------------------------------------------------------------*/
#if defined(TCNT3) && RTIMER_ARCH_PRESCALER
ISR (TIMER3_COMPA_vect) {
  printf("\n\n---------------call ISR3---------------\n\n");
  DEBUGFLOW('/');
  ENERGEST_ON(ENERGEST_TYPE_IRQ);

  /* Disable rtimer interrupts */
  ETIMSK &= ~((1 << OCIE3A) | (1 << OCIE3B) | (1 << TOIE3) |
      (1 << TICIE3) | (1 << OCIE3C));

#if RTIMER_CONF_NESTED_INTERRUPTS /* default: this is not used */
  /* Enable nested interrupts. Allows radio interrupt during rtimer interrupt. */
  /* All interrupts are enabled including recursive rtimer, so use with caution */
  sei();
#endif




  /**
    *   start edited by ysk
    */
  /* interruption is here */
  struct process *p;


  // TODO delete
  printf("\n------------------\n");
  for(p = process_list; p != NULL; p = p->next) {
    printf("process - id: %d - state: %d - type: %d\n",
            p->id, p->state, p->type);
  }
  printf("------------------\n");
    printf("process current - id: %d - state: %d - type: %d\n",
            process_current->id, process_current->state, process_current->type);
    printf("process preempted - id: %d - state: %d - type: %d\n",
            process_preempted->id, process_preempted->state, process_preempted->type);
  printf("------------------\n\n");
  // TODO delete

  
  process_switch_preempted();
  //for(p = process_list; p != NULL; p = p->next) {
  //  if(p->id == process_current->id) {
  //    process_preempted = p;

  //    printf("process - id: %d - state: %d - type: %d\n",
  //            p->id, p->state, p->type);
  //    init_process_stack(p, p->pt.lc, main_thread_stack, STACK_SIZE);
  //    process_stack_ptr = p->stack_ptr;
  //  } /* if process id end */

  //  if(p->type == REALTIME_TASK) {
  //    if(process_suspend(p) == PROCESS_ERR_OK) {
  //      break;
  //    } /* if suspend end  */
  //  } /* if check type end */
  //} /* for end  */


  // TODO delete
  printf("\n------------------\n");
  for(p = process_list; p != NULL; p = p->next) {
    printf("process - id: %d - state: %d - type: %d\n",
            p->id, p->state, p->type);
  }
  printf("------------------\n");
    printf("process current - id: %d - state: %d - type: %d\n",
            process_current->id, process_current->state, process_current->type);
    printf("process preempted - id: %d - state: %d - type: %d\n",
            process_preempted->id, process_preempted->state, process_preempted->type);
  printf("------------------\n\n");
  // TODO delete


  SAVE_CONTEXT();
  printf("saved main\n");


  for(p = process_list; p != NULL; p = p->next) {
    if(p->type == REALTIME_TASK) {
      process_stack_ptr = p->stack_ptr;
    } /* if end */
  } /* for end  */


  RESTORE_CONTEXT();
  printf("restore sub\n");
  /**
    *   end edited by ysk
    */


  /* Call rtimer callback */
  printf("start rtimer next\n");
  rtimer_run_next();
  printf("finish rtimer next\n");


  /**
    *   start edited by ysk
    */
  /* interruption is here */
  SAVE_CONTEXT();
  printf("saved sub\n");


  // TODO delete
  printf("\n------------------\n");
  for(p = process_list; p != NULL; p = p->next) {
    printf("process - id: %d - state: %d - type: %d\n",
            p->id, p->state, p->type);
  }
  printf("------------------\n");
    printf("process current - id: %d - state: %d - type: %d\n",
            process_current->id, process_current->state, process_current->type);
    printf("process preempted - id: %d - state: %d - type: %d\n",
            process_preempted->id, process_preempted->state, process_preempted->type);
  printf("------------------\n\n");
  // TODO delete


  /* interruption is here */
  process_switch_called();
  /* ---------------- */
  //for(p = process_list; p != NULL; p = p->next) {
  //  //if(p->state == PROCESS_STATE_CALLED) {
  //  if(p->id == process_current->id) {
  //    printf("if restore main in\n");
  //    process_stack_ptr = p->stack_ptr;
  //  }
  //}
  /* ---------------- */
  //for(p = process_list; p != NULL; p = p->next) {
  //  if(p->type == REALTIME_TASK) {
  //    if(process_resume(p) == PROCESS_ERR_OK) {
  //      break;
  //    }
  //  }

  //  if(p->id == process_preempted->id) {
  //    process_stack_ptr = p->stack_ptr;
  //    process_preempted = NULL;
  //  }
  //} /* for end */
  /* ---------------- */


  
  // TODO delete
  printf("\n------------------\n");
  for(p = process_list; p != NULL; p = p->next) {
    printf("process - id: %d - state: %d - type: %d\n",
            p->id, p->state, p->type);
  }
  printf("------------------\n");
    printf("process current - id: %d - state: %d - type: %d\n",
            process_current->id, process_current->state, process_current->type);
    printf("process preempted - id: %d - state: %d - type: %d\n",
            process_preempted->id, process_preempted->state, process_preempted->type);
  printf("------------------\n\n");
  // TODO delete


  RESTORE_CONTEXT();
  printf("restore main\n");
  /**
    *   end edited by ysk
    */




  ENERGEST_OFF(ENERGEST_TYPE_IRQ);
  DEBUGFLOW('\\');
  printf("\n\n---------------end ISR3---------------\n\n");
}

#elif RTIMER_ARCH_PRESCALER
#warning "No Timer3 in rtimer-arch.c - using Timer1 instead"
ISR (TIMER1_COMPA_vect) {
  printf("\n\n---------------call ISR1---------------\n\n");
  DEBUGFLOW('/');
  TIMSK &= ~((1<<TICIE1)|(1<<OCIE1A)|(1<<OCIE1B)|(1<<TOIE1));

  rtimer_run_next();
  DEBUGFLOW('\\');
}

#endif
/*---------------------------------------------------------------------------*/
void
rtimer_arch_init(void)
{
#if RTIMER_ARCH_PRESCALER
  /* Disable interrupts (store old state) */
  uint8_t sreg;
  sreg = SREG;
  cli ();

#ifdef TCNT3
  /* Disable all timer functions */
  ETIMSK &= ~((1 << OCIE3A) | (1 << OCIE3B) | (1 << TOIE3) |
      (1 << TICIE3) | (1 << OCIE3C));
  /* Write 1s to clear existing timer function flags */
  ETIFR |= (1 << ICF3) | (1 << OCF3A) | (1 << OCF3B) | (1 << TOV3) |
  (1 << OCF3C); 

  /* Default timer behaviour */
  TCCR3A = 0;
  TCCR3B = 0;
  TCCR3C = 0;

  /* Reset counter */
  TCNT3 = 0;

#if RTIMER_ARCH_PRESCALER==1024
  TCCR3B |= 5;
#elif RTIMER_ARCH_PRESCALER==256
  TCCR3B |= 4;
#elif RTIMER_ARCH_PRESCALER==64
  TCCR3B |= 3;
#elif RTIMER_ARCH_PRESCALER==8
  TCCR3B |= 2;
#elif RTIMER_ARCH_PRESCALER==1
  TCCR3B |= 1;
#else
#error Timer3 PRESCALER factor not supported.
#endif

#elif RTIMER_ARCH_PRESCALER
  /* Leave timer1 alone if PRESCALER set to zero */
  /* Obviously you can not then use rtimers */

  TIMSK &= ~((1<<TICIE1)|(1<<OCIE1A)|(1<<OCIE1B)|(1<<TOIE1));
  TIFR |= (1 << ICF1) | (1 << OCF1A) | (1 << OCF1B) | (1 << TOV1);

  /* Default timer behaviour */
  TCCR1A = 0;
  TCCR1B = 0;

  /* Reset counter */
  TCNT1 = 0;

  /* Start clock */
#if RTIMER_ARCH_PRESCALER==1024
  TCCR1B |= 5;
#elif RTIMER_ARCH_PRESCALER==256
  TCCR1B |= 4;
#elif RTIMER_ARCH_PRESCALER==64
  TCCR1B |= 3;
#elif RTIMER_ARCH_PRESCALER==8
  TCCR1B |= 2;
#elif RTIMER_ARCH_PRESCALER==1
  TCCR1B |= 1;
#else
#error Timer1 PRESCALER factor not supported.
#endif

#endif /* TCNT3 */

  /* Restore interrupt state */
  SREG = sreg;
#endif /* RTIMER_ARCH_PRESCALER */
}
/*---------------------------------------------------------------------------*/
void
rtimer_arch_schedule(rtimer_clock_t t)
{
  printf("\nstart rtimer_arch_schedule\n\n");
#if RTIMER_ARCH_PRESCALER
  /* Disable interrupts (store old state) */
  uint8_t sreg;
  sreg = SREG;
  cli ();
  DEBUGFLOW(':');


  

  //struct process *p;

  //if(chcontext_count % 2 == 0) {
  //  for(p = process_list; p != NULL; p = p->next) {
  //    //if(p->state == PROCESS_STATE_CALLED) {
  //    if(p->id == 2) {
  //      //init_process_stack(p, p->pt.lc, main_thread_stack, STACK_SIZE);
  //      process_stack_ptr = p->stack_ptr;

  //      //if(chcontext_count == 0) {
  //      //  RESTORE_CONTEXT();
  //      //}
  //    } /* if end */
  //  } /* for end  */
  //}
  //else if(chcontext_count % 2 == 1) {
  //  for(p = process_list; p != NULL; p = p->next) {
  //    if(p->type == REALTIME_TASK) {
  //      process_stack_ptr = p->stack_ptr;

  //      //if(chcontext_count == 1) {
  //      //  RESTORE_CONTEXT();
  //      //}
  //    } /* if end */
  //  } /* for end  */
  //}

  //printf("process_stack_ptr: %p - value: %d\n", &process_stack_ptr, process_stack_ptr);
  //SAVE_CONTEXT();

  //if(chcontext_count % 2 == 0) {
  //  for(p = process_list; p != NULL; p = p->next) {
  //    //if(p->state == PROCESS_STATE_CALLED) {
  //    if(p->type == REALTIME_TASK) {
  //      process_stack_ptr = p->stack_ptr;
  //    } /* if end */
  //  } /* for end  */
  //}
  //else if(chcontext_count % 2 == 1) {
  //  for(p = process_list; p != NULL; p = p->next) {
  //    //if(p->state == PROCESS_STATE_CALLED) {
  //    if(p->id == 2) {
  //      init_process_stack(p, p->pt.lc, main_thread_stack, STACK_SIZE);
  //      process_stack_ptr = p->stack_ptr;
  //    } /* if end */
  //  } /* for end  */
  //}

  //printf("process_stack_ptr: %p - value: %d\n", &process_stack_ptr, process_stack_ptr);
  //RESTORE_CONTEXT();
  ////asm volatile ("reti");

  //chcontext_count++;



  /*
   *
   */
  //struct process *p;

  //if(chcontext_count == 0) {
  //}
  ///* sub threads data is saved */
  //else if(chcontext_count % 2 == 0) {
  //  printf("save sub\n");
  //  SAVE_CONTEXT();

  //  for(p = process_list; p != NULL; p = p->next) {
  //    if(p->type == REALTIME_TASK) {
  //      process_resume(p);
  //      printf("resume process current: - state: %d - type: %d\n", 
  //              process_current->state, process_current->type);
  //    } /* if end */
  //  } /* for end  */
  //}
  ///* main threads data is saved */
  //else if(chcontext_count % 2 == 1) {
  //  printf("save main\n");

  //  for(p = process_list; p != NULL; p = p->next) {
  //    //if(p->state == PROCESS_STATE_CALLED) {
  //    if(p->id == process_current->id) {
  //      init_process_stack(p, p->pt.lc, main_thread_stack, STACK_SIZE);
  //      //printf("process_stack_ptr: %p - value: %d\n", &process_stack_ptr, process_stack_ptr);
  //      process_stack_ptr = p->stack_ptr;
  //      //printf("process_stack_ptr: %p - value: %d\n", &process_stack_ptr, process_stack_ptr);
  //    } /* if end */
  //  } /* for end  */

  //  SAVE_CONTEXT();

  //  for(p = process_list; p != NULL; p = p->next) {
  //    //printf("process: - id: %d - state: %d - type: %d\n", 
  //    //        p->id, p->state, p->type);
  //    if(p->type == REALTIME_TASK) {
  //      process_suspend(p);
  //      //printf("suspend process current: - state: %d - type: %d\n", 
  //      //        process_current->state, process_current->type);
  //    } /* if end */
  //  } /* for end  */
  //}
  //else {
  //  printf("< UNEXPECTED ERROR HAS OCCURRED >\n");
  //  return;
  //}

  //chcontext_count = chcontext_count + 1;
  ///*
  // *
  // */



  ///*
  // *
  // */
  //if(chcontext_count == 1) {
  //}
  ////else if(chcontext_count % 2 == 1) {
  ////  printf("2 restore\n");
  ////  RESTORE_CONTEXT();
  ////  printf("2 end restore\n");
  ////  asm volatile ("reti");
  ////}
  //else {
  //  printf("restore\n");
  //  RESTORE_CONTEXT();
  //  printf("end restore\n");
  //}
  /*
   *
   */




#ifdef TCNT3    /* default: this is executed */
  /* Set compare register */
  OCR3A = t;
  /* Write 1s to clear all timer function flags */
  ETIFR |= (1 << ICF3) | (1 << OCF3A) | (1 << OCF3B) | (1 << TOV3) |
  (1 << OCF3C);
  /* Enable interrupt on OCR3A match */
  ETIMSK |= (1 << OCIE3A);

#elif RTIMER_ARCH_PRESCALER
  /* Set compare register */
  OCR1A = t;
  TIFR |= (1 << ICF1) | (1 << OCF1A) | (1 << OCF1B) | (1 << TOV1);
  TIMSK |= (1 << OCIE1A);

#endif

  /* Restore interrupt state */
  SREG = sreg;
#endif /* RTIMER_ARCH_PRESCALER */

  printf("\nfinish rtimer_arch_schedule\n\n");
}

#if RDC_CONF_MCU_SLEEP
/*---------------------------------------------------------------------------*/
void
rtimer_arch_sleep(rtimer_clock_t howlong)
{
/* Deep Sleep for howlong rtimer ticks. This will stop all timers except
 * for TIMER2 which can be clocked using an external crystal.
 * Unfortunately this is an 8 bit timer; a lower prescaler gives higher
 * precision but smaller maximum sleep time.
 * Here a maximum 128msec (contikimac 8Hz channel check sleep) is assumed.
 * The rtimer and system clocks are adjusted to reflect the sleep time.
 */
#include <avr/sleep.h>
#include <dev/watchdog.h>
uint32_t longhowlong;
#if AVR_CONF_USE32KCRYSTAL
/* Save TIMER2 configuration if clock.c is using it */
    uint8_t savedTCNT2=TCNT2, savedTCCR2A=TCCR2A, savedTCCR2B = TCCR2B, savedOCR2A = OCR2A;
#endif
    cli();
	watchdog_stop();
	set_sleep_mode(SLEEP_MODE_PWR_SAVE);

/* Set TIMER2 clock asynchronus from external source, CTC mode */
    ASSR |= (1 << AS2);
    TCCR2A =(1<<WGM21);
/* Set prescaler and TIMER2 output compare register */
#if 0    //Prescale by 1024 -   32 ticks/sec, 8 seconds max sleep
    TCCR2B =((1<<CS22)|(1<<CS21)|(1<<CS20));
	longhowlong=howlong*32UL; 
#elif 0  // Prescale by 256 -  128 ticks/sec, 2 seconds max sleep
	TCCR2B =((1<<CS22)|(1<<CS21)|(0<<CS20));
	longhowlong=howlong*128UL;
#elif 0  // Prescale by 128 -  256 ticks/sec, 1 seconds max sleep
	TCCR2B =((1<<CS22)|(0<<CS21)|(1<<CS20));
	longhowlong=howlong*256UL;
#elif 0  // Prescale by  64 -  512 ticks/sec, 500 msec max sleep
	TCCR2B =((1<<CS22)|(0<<CS21)|(0<<CS20));
	longhowlong=howlong*512UL;
#elif 1  // Prescale by  32 - 1024 ticks/sec, 250 msec max sleep
	TCCR2B =((0<<CS22)|(1<<CS21)|(1<<CS20));
	longhowlong=howlong*1024UL;
#elif 0  // Prescale by   8 - 4096 ticks/sec, 62.5 msec max sleep
	TCCR2B =((0<<CS22)|(1<<CS21)|(0<<CS20));
	longhowlong=howlong*4096UL;
#else    // No Prescale -    32768 ticks/sec, 7.8 msec max sleep
	TCCR2B =((0<<CS22)|(0<<CS21)|(1<<CS20));
	longhowlong=howlong*32768UL;
#endif
	OCR2A = longhowlong/RTIMER_ARCH_SECOND;

/* Reset timer count, wait for the write (which assures TCCR2x and OCR2A are finished) */
    TCNT2 = 0; 
    while(ASSR & (1 << TCN2UB));

/* Enable TIMER2 output compare interrupt, sleep mode and sleep */
    TIMSK2 |= (1 << OCIE2A);
    SMCR |= (1 <<  SE);
	sei();
	ENERGEST_OFF(ENERGEST_TYPE_CPU);
	if (OCR2A) sleep_mode();
	  //...zzZZZzz...Ding!//

/* Disable sleep mode after wakeup, so random code cant trigger sleep */
    SMCR  &= ~(1 << SE);

/* Adjust rtimer ticks if rtimer is enabled. TIMER3 is preferred, else TIMER1 */
#if RTIMER_ARCH_PRESCALER
#ifdef TCNT3
    TCNT3 += howlong;
#else
    TCNT1 += howlong;
#endif
#endif
	ENERGEST_ON(ENERGEST_TYPE_CPU);

#if AVR_CONF_USE32KCRYSTAL
/* Restore clock.c configuration */
    cli();
    TCCR2A = savedTCCR2A;
    TCCR2B = savedTCCR2B;
    OCR2A  = savedOCR2A;
    TCNT2  = savedTCNT2;
    sei();
#else
/* Disable TIMER2 interrupt */
    TIMSK2 &= ~(1 << OCIE2A);
#endif
    watchdog_start();

/* Adjust clock.c for the time spent sleeping */
	longhowlong=CLOCK_CONF_SECOND;
	longhowlong*=howlong;
    clock_adjust_ticks(longhowlong/RTIMER_ARCH_SECOND);

}
#if !AVR_CONF_USE32KCRYSTAL
/*---------------------------------------------------------------------------*/
/* TIMER2 Interrupt service */

ISR(TIMER2_COMPA_vect)
{
  printf("\n\n---------------call ISR2---------------\n\n");
//    TIMSK2 &= ~(1 << OCIE2A);       //Just one interrupt needed for waking
}
#endif /* !AVR_CONF_USE32KCRYSTAL */
#endif /* RDC_CONF_MCU_SLEEP */
/*---------------------------------------------------------------------------*/
