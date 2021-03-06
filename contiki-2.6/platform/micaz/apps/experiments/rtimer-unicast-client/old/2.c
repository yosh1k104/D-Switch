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

#include "dev/battery-sensor.h"
#include "lib/sensors.h"
#include "dev/leds.h"

#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------*/
static struct rtimer my_timer;

PROCESS(example_unicast_process, "Example unicast");
PROCESS(blink_yellow_process, "LED blink yellow process");

AUTOSTART_PROCESSES(
  &example_unicast_process,
  &blink_yellow_process
);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
  uint32_t count = 0;

  leds_on(LEDS_RED);
  clock_delay(400);
  leds_off(LEDS_RED);

  printf("unicast message received from %d.%d: '%s'\n",
	 from->u8[0], from->u8[1], (char *)packetbuf_dataptr());
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
static rimeaddr_t addr;
static int send_ret;
/*---------------------------------------------------------------------------*/
static char periodic_rtimer(struct rtimer *rt, void* ptr)
{
  printf("unko\n");
  //char packet[10];

  //sprintf(packet, "%d", battery_sensor.value(0));
  //packetbuf_copyfrom(packet, strlen(packet) + 1);
  packetbuf_copyfrom("hello", 6);
  addr.u8[0] = 23;
  addr.u8[1] = 229;

  printf("aho\n");


  if(!rimeaddr_cmp(&addr, &rimeaddr_node_addr)) {
    send_ret = unicast_send(&uc, &addr);

    leds_on(LEDS_GREEN);
    clock_delay(400);
    leds_off(LEDS_GREEN);

    printf("unicast message sent\n\n");
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  SENSORS_ACTIVATE(battery_sensor);
  realtime_task_init(&example_unicast_process, 
                    (void (*)(struct rtimer *, void *))periodic_rtimer);

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    PROCESS_YIELD();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(blink_yellow_process, ev, data)
{
  static struct etimer timer;
  unsigned short ex_rand;
  uint8_t ret;

  PROCESS_BEGIN();

  while(1) {
    etimer_set(&timer, CLOCK_CONF_SECOND * 2);

    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    uint32_t count = 0;

    ex_rand = random_rand();

    printf("random: %d\n\n", ex_rand); 

    //if(ex_rand % 5 == 0) {
      leds_on(LEDS_YELLOW);
      clock_delay(400);
      leds_off(LEDS_YELLOW);

      printf("hoge\n");

      ret = rtimer_set(&my_timer, RTIMER_NOW() + RTIMER_SECOND / 1000, 1,
            (void (*)(struct rtimer *, void *))periodic_rtimer, NULL);

      printf("moge\n");

      if(ret) {
        printf("Error Timer: %u\n", ret);
      }
    //}
  }
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
