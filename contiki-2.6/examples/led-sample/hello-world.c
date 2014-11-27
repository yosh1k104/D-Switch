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

#include <stdio.h> /* For printf() */

#define RED_INTERVAL 2
#define BLUE_INTERVAL 3
#define GREEN_INTERVAL 4
/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
PROCESS(blink_red_process, "LED blink red process");
PROCESS(blink_blue_process, "LED blink blue process");
PROCESS(blink_green_process, "LED blink green process");
AUTOSTART_PROCESSES(&hello_world_process,
                    &blink_red_process,
                    &blink_blue_process,
                    &blink_green_process
);
/*---------------------------------------------------------------------------*/
/* Implementation of the first process */
PROCESS_THREAD(hello_world_process, ev, data)
{
    // variables are declared static to ensure their values are kept
    // between kernel calls.
    static struct etimer timer;
    static int count = 0;
    
    // any process mustt start with this.
    PROCESS_BEGIN();
    
    // set the etimer module to generate an event in one second.
    etimer_set(&timer, CLOCK_CONF_SECOND);
    while (1)
    {
        // wait here for an event to happen
        PROCESS_WAIT_EVENT();

        printf("\n----------Got event number %d in hello_world-----------\n", ev);

        // if the event is the timer event as expected...
        if(ev == PROCESS_EVENT_TIMER)
        {
            // do the process work
            printf("Hello, world: %d\n\n", count);
            count ++;
            
            // reset the timer so it will generate an other event
            // the exact same time after it expired (periodicity guaranteed)
            etimer_reset(&timer);
        }
        
        // and loop
    }
    // any process must end with this, even if it is never reached.
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Implementation of the second process */
PROCESS_THREAD(blink_red_process, ev, data)
{
    static struct etimer timer;
    static uint8_t leds_state = 0;
    static uint32_t now = 0;
    PROCESS_BEGIN();
    
    while (1)
    {
        // we set the timer from here every time
        etimer_set(&timer, CLOCK_CONF_SECOND / 2);
        
        // and wait until the vent we receive is the one we're waiting for
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

        printf("=clock_seconds: %lu=\n", clock_seconds());
        printf("\n----------Got event number %d in blink_red-----------\n\n", ev);
        
       // leds_off(LEDS_ALL);
        leds_on(LEDS_RED);

        now = clock_seconds();
        while(clock_seconds() < (now + RED_INTERVAL)){
        }

        leds_off(LEDS_RED);

    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Implementation of the third process */
PROCESS_THREAD(blink_blue_process, ev, data)
{
    static struct etimer timer;
    static uint8_t leds_state = 0;
    static uint32_t now = 0;
    PROCESS_BEGIN();
    
    while (1)
    {
        // we set the timer from here every time
        etimer_set(&timer, CLOCK_CONF_SECOND / 6);
        
        // and wait until the vent we receive is the one we're waiting for
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

        printf("=clock_seconds: %lu=\n", clock_seconds());
        printf("\n----------Got event number %d in blink_blue-----------\n\n", ev);
        
        //leds_on(LEDS_ALL);
        leds_on(LEDS_BLUE);

        now = clock_seconds();
        while(clock_seconds() < (now + BLUE_INTERVAL)){
        }

        leds_off(LEDS_BLUE);

    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* Implementation of the fourth process */
PROCESS_THREAD(blink_green_process, ev, data)
{
    static struct etimer timer;
    static uint8_t leds_state = 0;
    static uint32_t now = 0;
    PROCESS_BEGIN();
    
    while (1)
    {
        // we set the timer from here every time
        etimer_set(&timer, CLOCK_CONF_SECOND / 10);
        
        // and wait until the vent we receive is the one we're waiting for
        PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

        printf("=clock_seconds: %lu=\n", clock_seconds());
        printf("\n----------Got event number %d in blink_green-----------\n\n", ev);
        
        //leds_on(LEDS_ALL);
        leds_on(LEDS_GREEN);

        now = clock_seconds();
        while(clock_seconds() < (now + GREEN_INTERVAL)){
        }

        leds_off(LEDS_GREEN);

    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
