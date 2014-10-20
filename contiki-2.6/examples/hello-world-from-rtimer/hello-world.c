/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
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
 * $Id: hello-world.c,v 1.1 2006/10/02 21:46:46 adamdunkels Exp $
 */

/**
 * \file
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"  
#include <stdio.h>  
#include "sys/rtimer.h"  
#define     PERIOD_T     5*RTIMER_SECOND  
  
static struct rtimer my_timer;  
  
PROCESS(hello_world_process, "Hello world process");  
AUTOSTART_PROCESSES(&hello_world_process);  
   
 // the function which gets called each time the rtimer triggers  
 static char periodic_rtimer(struct rtimer *rt, void* ptr){  
   uint8_t ret;  
   rtimer_clock_t time_now = RTIMER_NOW();  
   
   printf("Hello from rtimer!!!\n");  
   
   ret = rtimer_set(&my_timer, time_now + PERIOD_T, 1,   
         (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);  
   if(ret){  
    printf("Error Timer: %u\n", ret);  
   }  
   return 1;  
 }  
   
 PROCESS_THREAD(hello_world_process, ev, data)  
 {  
  PROCESS_BEGIN();  
   
  printf("Starting the application...\n");  
   
  periodic_rtimer(&my_timer, NULL);  
   
  while(1){             
   PROCESS_YIELD();  
  }  
  PROCESS_END();  
 }  
