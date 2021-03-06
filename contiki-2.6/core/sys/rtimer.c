/**
 * \addtogroup rt
 * @{
 */

/**
 * \file
 *         Implementation of the architecture-agnostic parts of the real-time timer module.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */


/*
 * Copyright (c) 2005, Swedish Institute of Computer Science
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
 * @(#)$Id: rtimer.c,v 1.7 2010/01/19 13:08:24 adamdunkels Exp $
 */

#include "sys/rtimer.h"
#include "contiki.h"

#define PROCESS_STATE_CALLED    2

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

static struct rtimer *next_rtimer;
//static struct process *realtime_process = NULL;

/*---------------------------------------------------------------------------*/
void
rtimer_init(void)
{
  rtimer_arch_init();
}
/*---------------------------------------------------------------------------*/
int
realtime_task_init(struct process *p, rtimer_callback_t func)
{
  init_process_stack(p, func, sub_thread_stack, STACK_SIZE);
  p->type = REALTIME_TASK;
  process_realtime = p;

  return 1;
}
/*---------------------------------------------------------------------------*/
int
rtimer_set(struct rtimer *rtimer, rtimer_clock_t time,
	   rtimer_clock_t duration,
	   rtimer_callback_t func, void *ptr)
{
  //static int set_count = 0;

  //struct process *p; 
  //for(p = process_list; p != NULL; p = p->next) {
  //  if((p->state == PROCESS_STATE_CALLED)  && (set_count == 0)) {
  //    init_process_stack(p, func, sub_thread_stack, STACK_SIZE);
  //    p->type = REALTIME_TASK;
  //    process_realtime = p;
  //  } /* if end */
  //} /* for end  */

  //printf("set count: %d\n", set_count);

  //set_count = set_count + 1;
  
  
   
  int first = 0;

  /** TODO for debug
  printf("rtimer_set time %d\n", time);
  printf("rtimer process current: %s\n", PROCESS_NAME_STRING(process_current));
  */
  //printf("rtimer realtime process: %s\n\n", PROCESS_NAME_STRING(process_realtime));

  if(next_rtimer == NULL) {
    first = 1;
  }

  rtimer->func = func;
  rtimer->ptr = ptr;

  rtimer->time = time;
  next_rtimer = rtimer;

  if(first == 1) {
    rtimer_arch_schedule(time);
  }
  return RTIMER_OK;
}
/*---------------------------------------------------------------------------*/
void
rtimer_run_next(void)
{
  //printf("\n---------------in rtimer-----------------\n");
  //struct process *tmp_p;
  //for(tmp_p = process_list; tmp_p != NULL; tmp_p = tmp_p->next) {
  //  printf("process: %s - state: %d\n", PROCESS_NAME_STRING(tmp_p), tmp_p->state);
  //}
  /** TODO for debug
  printf("\n---------------in rtimer-----------------\n");
  */

  struct rtimer *t;
  if(next_rtimer == NULL) {
    return;
  }
  t = next_rtimer;
  next_rtimer = NULL;
  /** TODO for debug
  printf("start here\n");
  */
  t->func(t, t->ptr);
  /** TODO for debug
  printf("end here\n");
  */
  if(next_rtimer != NULL) {
    rtimer_arch_schedule(next_rtimer->time);
  }

  /** TODO for debug
  printf("\n---------------end rtimer-----------------\n");
  */

  return;
}
/*---------------------------------------------------------------------------*/
//char
//suspend_flag_on()
//{
//  return SUSPEND_ON;
//}
/*---------------------------------------------------------------------------*/
//char
//suspend_flag_off()
//{
//  return SUSPEND_OFF;
//}
/*---------------------------------------------------------------------------*/
