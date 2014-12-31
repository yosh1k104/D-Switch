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
#include "dev/leds.h"
#include "sys/rtimer.h"

#include <stdio.h> /* For printf() */

#define RED_INTERVAL 1
#define GREEN_INTERVAL 3
#define YELLOW_INTERVAL 1

#define PERIOD_T 5*RTIMER_SECOND
/*---------------------------------------------------------------------------*/
static struct rtimer my_timer;

PROCESS(preemption_process, "Preemption process");
PROCESS(blink_red_process, "LED blink red process");
AUTOSTART_PROCESSES(&preemption_process,
                    &blink_red_process
);
/*---------------------------------------------------------------------------*/
/* the function which gets called each time the rtimer triggers */
static char periodic_rtimer(struct rtimer *rt, void* ptr)
{
    uint8_t ret;
    rtimer_clock_t time_now = RTIMER_NOW();
    //printf("\nRTIMER_NOW: %u\n\n", RTIMER_NOW());

    static uint8_t leds_state = 0;
    static uint32_t now = 0;
    uint8_t count = 0;

    //leds_on(LEDS_ALL);
    leds_on(LEDS_YELLOW);
    //printf("\n{   leds_on(YELLOW) @ %lu    }\n\n", clock_seconds());

    now = clock_seconds();
    //printf("now: %lu\n", now);
    while(clock_seconds() < (now + YELLOW_INTERVAL) &&
          count < 50)
    {
        //printf("<   leds_on(YELLOW)    in while loop    >\n");
        //printf("<   leds_on(YELLOW) @ %lu    in while loop    >\n", clock_seconds());
        printf("y");
        count++;
    }
    printf("\n");

    leds_off(LEDS_YELLOW);
    //printf("\n{   leds_off(YELLOW) @ %lu    }\n\n", clock_seconds());

    //printf("process_current: %s - state: %d - type: %d\n", PROCESS_NAME_STRING(process_current), process_current->state, process_current->preemptive_type);
    //printf("process_preempted: %s - state: %d - type: %d\n", PROCESS_NAME_STRING(process_preempted), process_preempted->state, process_preempted->preemptive_type);

    ret = rtimer_set(&my_timer, time_now + PERIOD_T, 1,
          (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);

    if (ret)
    {
        printf("Error Timer: %u\n", ret);
    }

    return 1;
}
/*---------------------------------------------------------------------------*/
/* Implementation of the first process */
PROCESS_THREAD(preemption_process, ev, data)
{
    PROCESS_BEGIN();

    //process_current->preemptive_type = PREEMPTIVE_OK;
    //printf("process_current: %s - state: %d - type: %d\n", PROCESS_NAME_STRING(process_current), process_current->state, process_current->preemptive_type);

    printf("Starting the application...\n");

    periodic_rtimer(&my_timer, NULL);

    while (1)
    {
        //printf("\n---------before---------\n");
        //struct process *tmp_p;
        //for(tmp_p = process_list; tmp_p != NULL; tmp_p = tmp_p->next) {
        //  printf("process: %s - state: %d\n", PROCESS_NAME_STRING(tmp_p), tmp_p->state);
        //}
        //printf("\n---------before---------\n");

        //printf("\n----------Got event number %d in blink_yellow-----------\n\n", ev);
        PROCESS_YIELD();

        //printf("\n---------after---------\n");
        //for(tmp_p = process_list; tmp_p != NULL; tmp_p = tmp_p->next) {
        //  printf("process: %s - state: %d\n", PROCESS_NAME_STRING(tmp_p), tmp_p->state);
        //}
        //printf("\n---------after---------\n");
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Implementation of the third process */
PROCESS_THREAD(blink_red_process, ev, data)
{
    static struct etimer timer;
    static uint8_t leds_state = 0;
    static uint32_t now = 0;
    PROCESS_BEGIN();
    
    while (1)
    {
        // we set the timer from here every time
        //etimer_set(&timer, CLOCK_CONF_SECOND / 2);
        etimer_set(&timer, CLOCK_CONF_SECOND * 2);
        
        // and wait until the vent we receive is the one we're waiting for
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

        leds_on(LEDS_RED);
        //printf("\n{   leds_on(RED) @ %lu    }\n\n", clock_seconds());

        printf("start\n");
        now = clock_seconds();
        while(clock_seconds() < (now + RED_INTERVAL)){
            //printf("<   leds_on(RED) @ %lu    in while loop    >\n", clock_seconds());
            //printf("<   leds_on(RED)    in while loop    >\n");
            printf("r");
        }
        printf("\nend\n\n");

        leds_off(LEDS_RED);
        //printf("\n{   leds_off(RED) @ %lu    }\n\n", clock_seconds());

    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
