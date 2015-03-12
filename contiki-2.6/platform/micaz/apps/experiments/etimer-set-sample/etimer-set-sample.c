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

#include "dev/battery-sensor.h"
#include "lib/sensors.h"
#include "dev/leds.h"

#include <stdio.h> /* For printf() */

//#define YELLOW_INTERVAL 1
#define INTERVAL 1000
/*---------------------------------------------------------------------------*/
static process_event_t EVENT_DATA_READY;

PROCESS(blink_yellow_process, "LED blink yellow process");
PROCESS(hello_world_process, "Hello world process");
PROCESS(blink_red_process, "LED blink red process");
PROCESS(blink_green_process, "LED blink green process");
PROCESS(blink_third_process, "LED blink third process");
PROCESS(blink_fourth_process, "LED blink fourth process");
PROCESS(blink_fifth_process, "LED blink fifth process");

AUTOSTART_PROCESSES(
  &blink_yellow_process,
  &hello_world_process,
  &blink_red_process,
  &blink_green_process
  //&blink_third_process,
  //&blink_fourth_process,
  //&blink_fifth_process
);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_yellow_process, ev, data)
{
  static struct etimer timer;
  static uint8_t leds_state = 0;
  //static uint32_t now = 0;

  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);

  while(1) {
    //etimer_set(&timer, CLOCK_CONF_SECOND * 2);

    //PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);
    //PROCESS_YIELD();
    PROCESS_WAIT_EVENT_UNTIL(ev == EVENT_DATA_READY);

    uint32_t count = 0;

    leds_on(LEDS_YELLOW);

    printf("ADC value : %d\n", battery_sensor.value(0));
    printf("start\n");
    //now = clock_seconds();
    //while(clock_seconds() < (now + YELLOW_INTERVAL) && count < 50) {
    while(count < INTERVAL) {
      printf("y");
      count++;
    }
    //printf("\n");
    printf("\nend\n\n");
    printf("ADC value : %d\n", battery_sensor.value(0));

    leds_off(LEDS_YELLOW);
    count = 0;
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
  static struct etimer timer;
  static int count = 0;
  unsigned short ex_rand;

  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);

  while(1) {
    etimer_set(&timer, CLOCK_CONF_SECOND / 4);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    ex_rand = random_rand();

    printf("Hello, world: %d\n", count); 
    printf("random: %d\n", ex_rand); 
    printf("ADC value : %d\n", battery_sensor.value(0));

    if(ex_rand % 10 == 0) {
      process_post(&blink_yellow_process, EVENT_DATA_READY, &data);
    }

    count++;
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_red_process, ev, data)
{
  static struct etimer timer;
  static uint8_t leds_state = 0;
  //static uint32_t now = 0;
  PROCESS_BEGIN();
  
  SENSORS_ACTIVATE(battery_sensor);

  while (1) {
    // we set the timer from here every time
    etimer_set(&timer, CLOCK_CONF_SECOND / 2);
    
    // and wait until the vent we receive is the one we're waiting for
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    static uint32_t count = 0;

    leds_on(LEDS_RED);
    //printf("\n{   leds_on(RED) @ %lu    }\n\n", clock_seconds());

    printf("ADC value : %d\n", battery_sensor.value(0));
    printf("start\n");
    while(count < INTERVAL) {
        printf("r");
        count++;
    }
    printf("\nend\n\n");
    printf("ADC value : %d\n", battery_sensor.value(0));

    leds_off(LEDS_RED);
    //printf("\n{   leds_off(RED) @ %lu    }\n\n", clock_seconds());
    count = 0;

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_green_process, ev, data)
{
  static struct etimer timer;
  static uint8_t leds_state = 0;
  //static uint32_t now = 0;
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);
  
  while (1) {
    // we set the timer from here every time
    etimer_set(&timer, CLOCK_CONF_SECOND / 3);
    
    // and wait until the vent we receive is the one we're waiting for
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    static uint32_t count = 0;

    leds_on(LEDS_GREEN);
    //printf("\n{   leds_on(RED) @ %lu    }\n\n", clock_seconds());

    printf("ADC value : %d\n", battery_sensor.value(0));
    printf("start\n");
    while(count < INTERVAL) {
        printf("g");
        count++;
    }
    printf("\nend\n\n");
    printf("ADC value : %d\n", battery_sensor.value(0));

    leds_off(LEDS_GREEN);
    //printf("\n{   leds_off(RED) @ %lu    }\n\n", clock_seconds());
    count = 0;

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_third_process, ev, data)
{
  static struct etimer timer;
  static uint8_t leds_state = 0;
  static uint32_t now = 0;
  PROCESS_BEGIN();
  
  while (1)
  {
    // we set the timer from here every time
    etimer_set(&timer, CLOCK_CONF_SECOND);
    
    // and wait until the vent we receive is the one we're waiting for
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    static uint32_t count = 0;

    leds_on(LEDS_GREEN);
    //printf("\n{   leds_on(RED) @ %lu    }\n\n", clock_seconds());

    printf("green start\n");
    while(count < INTERVAL) {
      printf("g");
      count++;
    }
    printf("\ngreen end\n\n");

    leds_off(LEDS_GREEN);
    //printf("\n{   leds_off(RED) @ %lu    }\n\n", clock_seconds());
    count = 0;

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_fourth_process, ev, data)
{
  static struct etimer timer;
  static uint8_t leds_state = 0;
  static uint32_t now = 0;
  PROCESS_BEGIN();
  
  while (1)
  {
    // we set the timer from here every time
    etimer_set(&timer, CLOCK_CONF_SECOND / 5);
    
    // and wait until the vent we receive is the one we're waiting for
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    static uint32_t count = 0;

    leds_on(LEDS_RED);
    //printf("\n{   leds_on(RED) @ %lu    }\n\n", clock_seconds());

    printf("red start\n");
    while(count < INTERVAL) {
      printf("r");
      count++;
    }
    printf("\nred end\n\n");

    leds_off(LEDS_RED);
    //printf("\n{   leds_off(RED) @ %lu    }\n\n", clock_seconds());
    count = 0;

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_fifth_process, ev, data)
{
  static struct etimer timer;
  static uint8_t leds_state = 0;
  static uint32_t now = 0;
  PROCESS_BEGIN();
  
  while (1)
  {
    // we set the timer from here every time
    etimer_set(&timer, CLOCK_CONF_SECOND * 2);
    
    // and wait until the vent we receive is the one we're waiting for
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    static uint32_t count = 0;

    leds_on(LEDS_RED);
    //printf("\n{   leds_on(RED) @ %lu    }\n\n", clock_seconds());

    printf("red start\n");
    while(count < INTERVAL) {
      printf("r");
      count++;
    }
    printf("\nred end\n\n");

    leds_off(LEDS_RED);
    //printf("\n{   leds_off(RED) @ %lu    }\n\n", clock_seconds());
    count = 0;

  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
