/******************************************************************************
*  Nano-RK, a real-time operating system for sensor networks.
*  Copyright (C) 2007, Real-Time and Multimedia Lab, Carnegie Mellon University
*  All rights reserved.
*
*  This is the Open Source Version of Nano-RK included as part of a Dual
*  Licensing Model. If you are unsure which license to use please refer to:
*  http://www.nanork.org/nano-RK/wiki/Licensing
*
*  This program is free software: you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation, version 2.0 of the License.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include <nrk.h>
#include <include.h>
#include <ulib.h>
#include <stdio.h>
#include <avr/sleep.h>
#include <hal.h>
#include <bmac.h>
#include <nrk_error.h>
#include <nrk_events.h>
#include <sampl.h>
#include <globals.h>
#include <nrk_eeprom.h>
#include <route_table.h>
#include <neighbor_list.h>
#include <debug.h>
#include <ff_lcd.h>
#include <lcd_driver.h>

nrk_task_type SW_TASK;
NRK_STK sw_task_stack[NRK_APP_STACKSIZE];
void sw_task(void);

int main()
{
  nrk_setup_ports();
  nrk_setup_uart(UART_BAUDRATE_115K2);

  nrk_init();

  nrk_led_clr(0);
  nrk_led_clr(1);
  nrk_led_clr(2);
  nrk_led_clr(3);

  nrk_time_set(0, 0);

  bmac_task_config();
  sampl_config();

  nrk_create_taskset();
  nrk_start();

  return 0;
}

void sampl_startup()
{
  lcd_setup();
}

void sw_task()
{
  while(1)
  {
    if(lcd_switch_pressed(1))
      lcd_string_array_cycle();
    else if(lcd_switch_pressed(2))
      lcd_string_array_cycle_reverse();

    nrk_wait_until_next_period();
  }
}

void nrk_create_taskset()
{
  SW_TASK.task = sw_task;
  nrk_task_set_stk(&SW_TASK, sw_task_stack, NRK_APP_STACKSIZE);
  SW_TASK.prio = 1;
  SW_TASK.FirstActivation = TRUE;
  SW_TASK.Type = BASIC_TASK;
  SW_TASK.SchType = PREEMPTIVE;
  SW_TASK.period.secs = 0;
  SW_TASK.period.nano_secs = 100 * NANOS_PER_MS;
  SW_TASK.cpu_reserve.secs = 1;
  SW_TASK.cpu_reserve.nano_secs = 50 * NANOS_PER_MS;
  SW_TASK.offset.secs = 0;
  SW_TASK.offset.nano_secs = 0;
  nrk_activate_task(&SW_TASK);
}
