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
 * @(#)$Id: process.c,v 1.12 2010/10/20 22:24:46 adamdunkels Exp $
 */

/**
 * \addtogroup process
 * @{
 */

/**
 * \file
 *         Implementation of the Contiki process kernel.
 * \author
 *         Adam Dunkels <adam@sics.se>
 *
 */

#include <stdio.h>

#include "sys/process.h"
#include "sys/arg.h"

/*
 * Pointer to the currently running process structure.
 */
struct process *process_list = NULL;
struct process *process_current = NULL;
struct process *process_preempted = NULL;  // +
struct process *process_realtime = NULL;  // +
 
static process_event_t lastevent;

/*
 * Structure used for keeping the queue of active events.
 */
struct event_data {
  process_event_t ev;
  process_data_t data;
  struct process *p;
};

static process_num_events_t nevents, fevent;
static struct event_data events[PROCESS_CONF_NUMEVENTS];
static uint8_t id_count = 1;

#if PROCESS_CONF_STATS
process_num_events_t process_maxevents;
#endif

static volatile unsigned char poll_requested;

#define PROCESS_STATE_NONE        0
#define PROCESS_STATE_RUNNING     1
#define PROCESS_STATE_CALLED      2
#define PROCESS_STATE_WAITING     3
#define PROCESS_STATE_READY       4
#define PROCESS_STATE_SUSPENDED   5
#define PROCESS_STATE_FINISHED    6


static void call_process(struct process *p, process_event_t ev, process_data_t data);

//int num_stacks = 0;


#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
process_event_t
process_alloc_event(void)
{
  return lastevent++;
}
/*---------------------------------------------------------------------------*/
void
process_start(struct process *p, const char *arg)
{
  struct process *q;

  /* First make sure that we don't try to start a process that is
     already running. */
  for(q = process_list; q != p && q != NULL; q = q->next);

  /* If we found the process on the process list, we bail out. */
  if(q == p) {
    return;
  }
  /* Put on the procs list.*/
  p->next = process_list;
  process_list = p;
  p->state = PROCESS_STATE_RUNNING;
  p->stack_ptr = NULL;
  p->type = NORMAL_TASK;
  p->id = id_count;
  id_count = id_count + 1;
  //set_process_type(p, NORMAL_TASK);
  PT_INIT(&p->pt);

  printf("process: starting '%s'\n", PROCESS_NAME_STRING(p));
  //printf("process: starting '%s' - type: %d\n", PROCESS_NAME_STRING(p), p->process_type);

  /* Post a synchronous initialization event to the process. */
  process_post_synch(p, PROCESS_EVENT_INIT, (process_data_t)arg);
}
/*---------------------------------------------------------------------------*/
static void
exit_process(struct process *p, struct process *fromprocess)
{
  register struct process *q;
  struct process *old_current = process_current;

  printf("process: exit_process '%s'\n", PROCESS_NAME_STRING(p));

  /* Make sure the process is in the process list before we try to
     exit it. */
  for(q = process_list; q != p && q != NULL; q = q->next);
  if(q == NULL) {
    return;
  }

  if(process_is_running(p)) {
    /* Process was running */
    p->state = PROCESS_STATE_NONE;

    /*
     * Post a synchronous event to all processes to inform them that
     * this process is about to exit. This will allow services to
     * deallocate state associated with this process.
     */
    for(q = process_list; q != NULL; q = q->next) {
      if(p != q) {
	call_process(q, PROCESS_EVENT_EXITED, (process_data_t)p);
      }
    }

    if(p->thread != NULL && p != fromprocess) {
      /* Post the exit event to the process that is about to exit. */
      process_current = p;
      p->thread(&p->pt, PROCESS_EVENT_EXIT, NULL);
    }
  }

  if(p == process_list) {
    process_list = process_list->next;
  } else {
    for(q = process_list; q != NULL; q = q->next) {
      if(q->next == p) {
	q->next = p->next;
	break;
      }
    }
  }

  process_current = old_current;
}
/*---------------------------------------------------------------------------*/
static void
call_process(struct process *p, process_event_t ev, process_data_t data)
{
  int ret;

#if DEBUG
  if(p->state == PROCESS_STATE_CALLED) {
    printf("process: process '%s' called again with event %d\n", PROCESS_NAME_STRING(p), ev);
  }
#endif /* DEBUG */

  if((p->state & PROCESS_STATE_RUNNING) &&
     p->thread != NULL) {
    if(ev == 129){
        //printf("process: calling process '%s' with event PROCESS_EVENT_INIT\n", PROCESS_NAME_STRING(p));
    }
    else if (ev == 130){
        //printf("process: calling process '%s' with event PROCESS_EVENT_POLL\n", PROCESS_NAME_STRING(p));
    }
    else if (ev == 136){
        //printf("process: calling process '%s' with event PROCESS_EVENT_TIMER\n", PROCESS_NAME_STRING(p));
    }
    process_current = p;
    p->state = PROCESS_STATE_CALLED;
    ret = p->thread(&p->pt, ev, data);
    if(ret == PT_EXITED ||
       ret == PT_ENDED ||
       ev == PROCESS_EVENT_EXIT) {
      exit_process(p, p);
    } else {
      p->state = PROCESS_STATE_RUNNING;
    }
  }
}
/*---------------------------------------------------------------------------*/
void
process_exit(struct process *p)
{
  exit_process(p, PROCESS_CURRENT());
}
/*---------------------------------------------------------------------------*/
void
process_init(void)
{
  lastevent = PROCESS_EVENT_MAX;

  nevents = fevent = 0;
#if PROCESS_CONF_STATS
  process_maxevents = 0;
#endif /* PROCESS_CONF_STATS */

  process_current = process_list = NULL;
}
/*---------------------------------------------------------------------------*/
/*
 * Call each process' poll handler.
 */
/*---------------------------------------------------------------------------*/
static void
do_poll(void)
{
  struct process *p;

  poll_requested = 0;
  /* Call the processes that needs to be polled. */
  for(p = process_list; p != NULL; p = p->next) {
    if(p->needspoll) {
      p->state = PROCESS_STATE_RUNNING;
      p->needspoll = 0;
      call_process(p, PROCESS_EVENT_POLL, NULL);
    }
  }
}
/*---------------------------------------------------------------------------*/
/*
 * Process the next event in the event queue and deliver it to
 * listening processes.
 */
/*---------------------------------------------------------------------------*/
static void
do_event(void)
{
  static process_event_t ev;
  static process_data_t data;
  static struct process *receiver;
  static struct process *p;
  
  /*
   * If there are any events in the queue, take the first one and walk
   * through the list of processes to see if the event should be
   * delivered to any of them. If so, we call the event handler
   * function for the process. We only process one event at a time and
   * call the poll handlers inbetween.
   */

  if(nevents > 0) {
    
    /* There are events that we should deliver. */
    ev = events[fevent].ev;
    
    data = events[fevent].data;
    receiver = events[fevent].p;

    /* Since we have seen the new event, we move pointer upwards
       and decrese the number of events. */
    fevent = (fevent + 1) % PROCESS_CONF_NUMEVENTS;
    --nevents;

    /* If this is a broadcast event, we deliver it to all events, in
       order of their priority. */
    if(receiver == PROCESS_BROADCAST) {
      for(p = process_list; p != NULL; p = p->next) {

	/* If we have been requested to poll a process, we do this in
	   between processing the broadcast event. */
	if(poll_requested) {
	  do_poll();
	}
	call_process(p, ev, data);
      }
    } else {
      /* This is not a broadcast event, so we deliver it to the
	 specified process. */
      /* If the event was an INIT event, we should also update the
	 state of the process. */
      if(ev == PROCESS_EVENT_INIT) {
	receiver->state = PROCESS_STATE_RUNNING;
      }

      /* Make sure that the process actually is running. */
      call_process(receiver, ev, data);
    }
  }
}
/*---------------------------------------------------------------------------*/
int
process_run(void)
{
  /* Process poll events. */
  if(poll_requested) {
    do_poll();
  }

  /* Process one event from the queue */
  do_event();

  return nevents + poll_requested;
}
/*---------------------------------------------------------------------------*/
int
process_nevents(void)
{
  return nevents + poll_requested;
}
/*---------------------------------------------------------------------------*/
int
process_post(struct process *p, process_event_t ev, process_data_t data)
{
  static process_num_events_t snum;

  if(PROCESS_CURRENT() == NULL) {
    printf("process_post: NULL process posts event %d to process '%s', nevents %d\n",
	   ev,PROCESS_NAME_STRING(p), nevents);
  } else {
    //printf("process_post: Process '%s' posts event %d to process '%s', nevents %d\n",
	//   PROCESS_NAME_STRING(PROCESS_CURRENT()), ev,
	//   p == PROCESS_BROADCAST? "<broadcast>": PROCESS_NAME_STRING(p), nevents);
  }
  
  if(nevents == PROCESS_CONF_NUMEVENTS) {
#if DEBUG
    if(p == PROCESS_BROADCAST) {
      printf("soft panic: event queue is full when broadcast event %d was posted from %s\n", ev, PROCESS_NAME_STRING(process_current));
    } else {
      printf("soft panic: event queue is full when event %d was posted to %s frpm %s\n", ev, PROCESS_NAME_STRING(p), PROCESS_NAME_STRING(process_current));
    }
#endif /* DEBUG */
    return PROCESS_ERR_FULL;
  }
  
  snum = (process_num_events_t)(fevent + nevents) % PROCESS_CONF_NUMEVENTS;
  events[snum].ev = ev;
  events[snum].data = data;
  events[snum].p = p;
  ++nevents;

#if PROCESS_CONF_STATS
  if(nevents > process_maxevents) {
    process_maxevents = nevents;
  }
#endif /* PROCESS_CONF_STATS */
  
  return PROCESS_ERR_OK;
}
/*---------------------------------------------------------------------------*/
void
process_post_synch(struct process *p, process_event_t ev, process_data_t data)
{
  struct process *caller = process_current;

  call_process(p, ev, data);
  process_current = caller;
}
/*---------------------------------------------------------------------------*/
void
process_poll(struct process *p)
{
  if(p != NULL) {
    if(p->state == PROCESS_STATE_RUNNING ||
       p->state == PROCESS_STATE_CALLED) {
      p->needspoll = 1;
      poll_requested = 1;
    }
  }
}
/*---------------------------------------------------------------------------*/
int
process_is_running(struct process *p)
{
  return p->state != PROCESS_STATE_NONE;
}
/*---------------------------------------------------------------------------*/
//void
//process_suspend(struct process *p)
//{
//  // TODO call init change stack function
//
//  static int count = 0;
//  if(count == 0) {
//      count++;
//      return;
//  }
//
//  char suspend_flag = 1;
//
//  struct process *tmp_p;
//  for(tmp_p = process_list; tmp_p != NULL; tmp_p = tmp_p->next) {
//    if(tmp_p->state == PROCESS_STATE_SUSPENDED) {
//      suspend_flag = 0;
//      break;
//    }
//  }
//
//  printf("\nsuspended\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("suspended\n\n");
//
//  if(suspend_flag == 1) {
//    process_current->state = PROCESS_STATE_SUSPENDED;
//  }
//  else {
//    process_current->state = PROCESS_STATE_RUNNING;
//  }
//
//  printf("\nsuspended\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("suspended\n\n");
//
//  process_preempted = process_current;
//  p->state = PROCESS_STATE_CALLED;
//  process_current = p;
//
//  printf("\nsuspended\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("process_preempted: %s - state: %d\n", PROCESS_NAME_STRING(process_preempted), process_preempted->state);
//  printf("suspended\n\n");
//
//}
/*---------------------------------------------------------------------------*/
//int 
//set_process_type(struct process *p, unsigned char type)
//{
//  p->process_type = type;
//  return p->process_type;
//}
/*---------------------------------------------------------------------------*/
//int
//process_suspend(struct process *p)
//{
//  if(process_preempted != NULL) {
//      printf("\nERROR SUSPENDED PROCESS HAS ALREADY EXISTED\n\n");
//      return PROCESS_ERR_SUSPEND;
//  }
//
//  if(process_current->state == PROCESS_STATE_RUNNING) {
//      printf("\nRUNNING PROCESS IS NONE\n\n");
//      return PROCESS_ERR_OK;
//  }
//
//
//
//  printf("\nsuspended\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("process_preempted: %s - state: %d\n", PROCESS_NAME_STRING(process_preempted), process_preempted->state);
//  //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
//  printf("suspended\n\n");
//
//
//
//  struct process *q;
//  for(q = process_list; q != NULL; q = q->next) {
//    if(q->state == PROCESS_STATE_SUSPENDED) {
//      printf("\nERROR SUSPENDED PROCESS HAS ALREADY EXISTED\n\n");
//      return PROCESS_ERR_SUSPEND;
//    }
//
//    if(q->state == PROCESS_STATE_CALLED) {
//      process_preempted = q;
//      q->state = PROCESS_STATE_SUSPENDED;
//      process_current = p;
//      p->state = PROCESS_STATE_CALLED;
//    } /* if end */
//  } /* for end */
//
//
//
//  printf("\nsuspended\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("process_preempted: %s - state: %d\n", PROCESS_NAME_STRING(process_preempted), process_preempted->state);
//  //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
//  printf("suspended\n\n");
//
//
//
//  return PROCESS_ERR_OK;
//}
///*---------------------------------------------------------------------------*/
//int
//process_resume(struct process *p)
//{
//  // TODO call init change stack function
//  if(process_preempted == NULL) {
//      printf("\nERROR PREEMPTED PROCESS IS NONE\n\n");
//      return PROCESS_ERR_RESUME;
//  }
//
//
//
//  printf("\nresume\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("process_preempted: %s - state: %d\n", PROCESS_NAME_STRING(process_preempted), process_preempted->state);
//  //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
//  printf("resume\n\n");
//
//
//
//  //int suspend_count = 0;
//
//  struct process *q;
//  for(q = process_list; q != NULL; q = q->next) {
//    if(q->state == PROCESS_STATE_SUSPENDED) {
//      //p = q;
//      //suspend_count = suspend_count + 1;
//      process_current = q;
//      q->state = PROCESS_STATE_CALLED;
//      process_preempted = NULL;
//      p->state = PROCESS_STATE_RUNNING;
//    } /* if end */
//  } /* for end  */
//
//
//
//  printf("\nresume\n");
//  printf("process_current: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//  printf("process_preempted: %s - state: %d\n", PROCESS_NAME_STRING(process_preempted), process_preempted->state);
//  //printf("process_realtime: %s - state: %d\n", PROCESS_NAME_STRING(process_realtime), process_realtime->state);
//  printf("resume\n\n");
//
//
//
//  //if(suspend_count != 1) {
//  //    printf("\nERROR SUSPENDED PROCESS IS NONE OR TOO MANY\n\n");
//  //    return PROCESS_ERR_RESUME;
//  //}
//
//  //printf("process_current1: %s - state: %d\n", PROCESS_NAME_STRING(PROCESS_CURRENT()), process_current->state);
//
//  return PROCESS_ERR_OK;
//}
/*---------------------------------------------------------------------------*/
//uint8_t 
//init_process_stack(struct process *p, char *task_ptr, uint8_t *buffer_ptr, uint16_t stack_size)
//{
//  printf("\nStart of init_process_stack\n\n");
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
//  printf("\nEnd of init_process_stack\n\n");
//
//  /* return the task id */
//  //return (num_stacks - 1);
//  return num_stacks;
//}
/*---------------------------------------------------------------------------*/
/** @} */
