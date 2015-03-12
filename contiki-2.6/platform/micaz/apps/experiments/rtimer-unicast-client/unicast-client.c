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
 * $Id: example-unicast.c,v 1.5 2010/02/02 16:36:46 adamdunkels Exp $
 */

/**
 * \file
 *         Best-effort single-hop unicast example
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"
#include "sys/rtimer.h"
#include "net/rime.h"
#include "random.h"

#include "dev/battery-sensor.h"
#include "lib/sensors.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
static struct rtimer my_timer;

PROCESS(example_unicast_process, "Example unicast");
PROCESS(example_unicast_process2, "Example unicast2");
PROCESS(example_unicast_process3, "Example unicast3");
PROCESS(example_unicast_process4, "Example unicast4");
PROCESS(blink_yellow_process, "LED blink yellow process");
PROCESS(blink_red_process1, "LED blink red process1");
PROCESS(blink_red_process2, "LED blink red process2");
PROCESS(blink_red_process3, "LED blink red process3");
PROCESS(blink_red_process4, "LED blink red process4");

AUTOSTART_PROCESSES(
  &example_unicast_process,
  &example_unicast_process2,
  //&example_unicast_process3,
  //&example_unicast_process4,
  &blink_yellow_process,
  &blink_red_process1,
  &blink_red_process2,
  &blink_red_process3
  //&blink_red_process4
);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
  leds_on(LEDS_RED);
  clock_delay(400);
  leds_off(LEDS_RED);

  printf("unicast message received from %d.%d: '%s'\n",
	 from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
static rimeaddr_t addr;
static char packet[10];
/*---------------------------------------------------------------------------*/
static char periodic_rtimer(struct rtimer *rt, void* ptr)
{
  //static rimeaddr_t addr;
  
  //packetbuf_copyfrom("Hello", 5);
  //addr.u8[0] = 23;
  //addr.u8[1] = 229;
  sprintf(packet, "%d", battery_sensor.value(0));
  packetbuf_copyfrom(packet, strlen(packet) + 1);

  if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
    unicast_send(&uc, &addr);

    leds_on(LEDS_GREEN);
    clock_delay(400);
    leds_off(LEDS_GREEN);

    printf("\nunicast message sent\n");
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  //uint8_t ret;

  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);

  realtime_task_init(&example_unicast_process, 
                    (void (*)(struct rtimer *, void *))periodic_rtimer);

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    //static struct etimer et;
    //rimeaddr_t addr;
    
    //etimer_set(&et, CLOCK_SECOND);
    
    //PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    //packetbuf_copyfrom("Hello", 5);
    //addr.u8[0] = 23;
    //addr.u8[1] = 229;

    //if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
    //  unicast_send(&uc, &addr);

    //  leds_on(LEDS_GREEN);
    //  clock_delay(400);
    //  leds_off(LEDS_GREEN);

    //  printf("unicast message sent\n");
    //}
    //periodic_rtimer(&my_timer, NULL);

    //ret = rtimer_set(&my_timer, RTIMER_NOW() + RTIMER_SECOND / 1000, 1,
    //      (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);

    /* For Micaz */
    addr.u8[0] = 45;
    addr.u8[1] = 185;

    /* For COOJA */
    //addr.u8[0] = 1;
    //addr.u8[1] = 0;

    PROCESS_YIELD();

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_yellow_process, ev, data)
{
  static struct etimer timer;
  uint8_t ret;

  PROCESS_BEGIN();

  while(1) {
    //etimer_set(&timer, CLOCK_CONF_SECOND * 2);
    /* Delay 2-4 seconds */
    etimer_set(&timer, CLOCK_SECOND * 2 + random_rand() % (CLOCK_SECOND * 2));

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    leds_on(LEDS_YELLOW);
    clock_delay(400);
    leds_off(LEDS_YELLOW);

    ret = rtimer_set(&my_timer, RTIMER_NOW() + RTIMER_SECOND / 1000, 1,
          (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);

    printf("Blink Yellow LED\n");
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_red_process1, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&timer, CLOCK_CONF_SECOND);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    leds_on(LEDS_RED);
    clock_delay(400);
    leds_off(LEDS_RED);

    printf("Blink RED LED1\n");
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_red_process2, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&timer, CLOCK_CONF_SECOND / 2);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    leds_on(LEDS_RED);
    clock_delay(400);
    leds_off(LEDS_RED);

    printf("Blink RED LED2\n");
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_red_process3, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&timer, CLOCK_CONF_SECOND / 3);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    leds_on(LEDS_RED);
    clock_delay(400);
    leds_off(LEDS_RED);

    printf("Blink RED LED3\n");
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_red_process4, ev, data)
{
  static struct etimer timer;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&timer, CLOCK_CONF_SECOND / 5);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    leds_on(LEDS_RED);
    clock_delay(400);
    leds_off(LEDS_RED);

    printf("Blink RED LED4\n");
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process2, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    static struct etimer et;
    rimeaddr_t addr;
    
    etimer_set(&et, CLOCK_SECOND / 2);
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    sprintf(packet, "%d", battery_sensor.value(0));
    packetbuf_copyfrom(packet, strlen(packet) + 1);

    addr.u8[0] = 45;
    addr.u8[1] = 185;

    if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
      unicast_send(&uc, &addr);

      leds_on(LEDS_GREEN);
      clock_delay(400);
      leds_off(LEDS_GREEN);

      printf("unicast message sent\n");
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process3, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    static struct etimer et;
    rimeaddr_t addr;
    
    etimer_set(&et, CLOCK_SECOND / 3);
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    sprintf(packet, "%d", battery_sensor.value(0));
    packetbuf_copyfrom(packet, strlen(packet) + 1);

    addr.u8[0] = 45;
    addr.u8[1] = 185;

    if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
      unicast_send(&uc, &addr);

      leds_on(LEDS_GREEN);
      clock_delay(400);
      leds_off(LEDS_GREEN);

      printf("unicast message sent\n");
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process4, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    static struct etimer et;
    rimeaddr_t addr;
    
    etimer_set(&et, CLOCK_SECOND);
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    sprintf(packet, "%d", battery_sensor.value(0));
    packetbuf_copyfrom(packet, strlen(packet) + 1);

    addr.u8[0] = 45;
    addr.u8[1] = 185;

    if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
      unicast_send(&uc, &addr);

      leds_on(LEDS_GREEN);
      clock_delay(400);
      leds_off(LEDS_GREEN);

      printf("unicast message sent\n");
    }

  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
