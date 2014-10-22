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
#include "contiki-net.h"

#include "dev/leds.h"
#include "net/rime/rimeaddr.h"
#include "net/rime/packetbuf.h"
#include "net/rime/broadcast.h"
#include "sys/rtimer.h"

#include "node-id.h"

#define DEBUG 1
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#define PRIOD_T  5*RTIMER_SECOND

static uint8_t counter = 0;
static struct rtimer my_timer;

PROCESS(myrtimer_process, "simple rtimer app");
AUTOSTART_PROCESSES(&myrtimer_process);

/*implements a counter,  the leds count from 0 to 7
 *param count - the value to be displayed of the leds
 */
static void leds_set(uint8_t count){

 //the shift is due to the change on the leds in the Tmote Sky platform
         if(count & LEDS_GREEN){
           leds_on(LEDS_GREEN<<1);
         }else{
            leds_off(LEDS_GREEN<<1);
         }

         if(count & LEDS_YELLOW){
           leds_on(LEDS_YELLOW>>1);
         }else{
           leds_off(LEDS_YELLOW>>1);
         }

         if(count & LEDS_RED){
          leds_on(LEDS_RED);
         }else{
          leds_off(LEDS_RED);
         }
}

/*---------------------------------------------------------------------------------*/
/*
You can change it using if/OR clauses to regulate the period... 
this is just an example
*/
static char periodic_rtimer(struct rtimer *rt, void* ptr){
     uint8_t ret;
     rtimer_clock_t time_now = RTIMER_NOW();

      leds_set(counter++);   //u gonna get the led counting from 0-7 

     ret = rtimer_set(&my_timer, time_now + PERIOD_T, 1, 
                (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);
     if(ret){
         PRINTF("Error Timer: %u\n", ret);
     }
   return 1;
}
/*----------------------------------------------------------------------------*/
PROCESS_THREAD(myrtimer_process, ev, data)
{

   PROCESS_EXITHANDLER(); 

   PROCESS_BEGIN();

   static struct etimer et;
   uint8_t ret;

   //you call the function here only once
   ret = rtimer_set(&my_timer, time_now + PERIOD_T, 1, 
                (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);
   if(ret){
         PRINTF("Error Timer: %u\n", ret);
   }

   while(1){                     
            PROCESS_YIELD(); //because we do not want to do anything in particular in this while loop
   }

   PROCESS_END();
}
/*----------------------------------------------------------------------------*/
