#include "contiki.h"
#include "net/rime.h"

#include "dev/battery-sensor.h"
#include "lib/sensors.h"
#include "dev/leds.h"

#include <stdio.h> /* For printf() */
/*---------------------------------------------------------------------------*/
PROCESS(energest_sample_process, "Energy get sample process");
PROCESS(example_unicast_process, "Example unicast");

AUTOSTART_PROCESSES(
  &energest_sample_process,
  &example_unicast_process
);
/*---------------------------------------------------------------------------*/
static void
recv_uc(struct unicast_conn *c, const rimeaddr_t *from)
{
  leds_on(LEDS_RED);
  clock_delay(400);
  leds_off(LEDS_RED);
 

  printf("unicast message received from %d.%d\n",
	 from->u8[0], from->u8[1]);
}
static const struct unicast_callbacks unicast_callbacks = {recv_uc};
static struct unicast_conn uc;
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(example_unicast_process, ev, data)
{
  PROCESS_EXITHANDLER(unicast_close(&uc);)
    
  PROCESS_BEGIN();

  unicast_open(&uc, 146, &unicast_callbacks);

  while(1) {
    static struct etimer et;
    rimeaddr_t addr;
    
    etimer_set(&et, CLOCK_SECOND);
    
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    packetbuf_copyfrom("Hello", 5);
    addr.u8[0] = 1;
    addr.u8[1] = 0;

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
PROCESS_THREAD(energest_sample_process, ev, data)
{
  static struct etimer et;
  static unsigned long rx_start_time;
  static unsigned long lpm_start_time;
  static unsigned long cpu_start_time;
  static unsigned long tx_start_time;

  PROCESS_BEGIN();
  
  /* here */
  while (1) {
    etimer_set(&et, CLOCK_CONF_SECOND * 2);
    PROCESS_WAIT_EVENT_UNTIL(ev == PROCESS_EVENT_TIMER);

    rx_start_time = energest_type_time(ENERGEST_TYPE_LISTEN);
    lpm_start_time = energest_type_time(ENERGEST_TYPE_LPM);
    cpu_start_time = energest_type_time(ENERGEST_TYPE_CPU);
    tx_start_time = energest_type_time(ENERGEST_TYPE_TRANSMIT);

    /* here */

    printf("energy listen %lu tx %lu cpu %lu lpm %lu\n",
    energest_type_time(ENERGEST_TYPE_LISTEN) - rx_start_time, // in while loop
    energest_type_time(ENERGEST_TYPE_TRANSMIT) - tx_start_time,
    energest_type_time(ENERGEST_TYPE_CPU) - cpu_start_time,
    energest_type_time(ENERGEST_TYPE_LPM) - lpm_start_time);
  }


  PROCESS_END();
}
