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
*  Contributing Authors (specific to this file):
*  John Yackovich
*  Anthony Rowe
*******************************************************************************/

#include <include.h>
#include <ulib.h>
#include <stdlib.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <nrk_eeprom.h>
#include <stdio.h>
#include <avr/interrupt.h>
#include <nrk.h>
#include <nrk_events.h>
#include <nrk_timer.h>
#include <nrk_error.h>
#include <nrk_reserve.h>
#include <tdma_asap.h>
#include <tdma_asap_scheduler.h>

//#define TDMA_TEXT_DEBUG


#define LED_BASIC_DEBUG
//#define GPIO_BASIC_DEBUG

//#define GPIO_SYNC_DEBUG
//#define GPIO_SLOT_DEBUG
//#define LED_DELTA_DEBUG
//#define GPIO_DELTA_DEBUG
//#define TDMA_SYNC_DEBUG

// monitor time of all radio use while tdma running
//#define TDMA_RADIO_TIME_DEBUG
//#define TDMA_AWAKE_TIME_DEBUG

#ifdef TDMA_RADIO_TIME_DEBUG
nrk_time_t radio_time;
nrk_time_t cur_radio_time;
nrk_time_t cur_radio_time2;
#endif

#ifdef TDMA_AWAKE_TIME_DEBUG
nrk_time_t  awake_time;
nrk_time_t  awake_time1;
nrk_time_t  awake_time2;
#endif

#ifdef TDMA_SYNC_DEBUG
    uint16_t sync_count = 0;
    uint16_t resync_count = 0;
    uint16_t oos_count = 0;
#endif 

uint8_t tdma_init_done = 0;
uint8_t tdma_running=0;

uint32_t mac_address;
uint16_t my_addr16;

nrk_time_t current_time;
nrk_time_t slot_start_time;
nrk_time_t guard_time;

tdma_slot_t sync_slot;
tdma_slot_t global_slot;
int8_t n;

tdma_node_mode_t tdma_node_mode;

// method of building the schedule.
tdma_sched_method_t tdma_schedule_method=TDMA_SCHED_MANUAL;

tdma_schedule_entry_t next_schedule_entry;

// Time of the last sync with a master
nrk_time_t sync_time;
nrk_time_t possible_sync_time;
nrk_time_t time_since_sync;

nrk_time_t sync_offset_time;
// freshness ctr
uint8_t tdma_time_token;

// time associated with beginning of current slot
nrk_time_t time_of_slot;

// Delta slot offset.  If I have lower priority on a TX slot
// I can wait to transmit
nrk_time_t delta_slot_offset;

static nrk_time_t timeout_time;
static nrk_time_t timeout_duration;
nrk_time_t twait_duration;

// Expiration period for a node to be in sync
// If a node has not sync'd for this long, it will be considered
// out of sync and will not be able to tx until sync operation
static nrk_time_t sync_tolerance_time;

// flag to note whether node is in sync
uint8_t in_sync;

int8_t rx_buf_empty;

uint8_t tdma_rx_buf[RF_MAX_PAYLOAD_SIZE];
uint8_t tdma_tx_buf[RF_MAX_PAYLOAD_SIZE];

uint8_t tdma_rx_data_ready;
uint8_t tdma_tx_data_ready;
uint16_t tdma_rx_slot;

// return the 16-bit version of my address
node_addr_t tdma_mac_get()
{
    if (tdma_running == 0)
        return NRK_ERROR;

    return my_addr16;
}

tdma_node_mode_t tdma_my_mode_get()
{
    return tdma_node_mode;
}

int8_t _tdma_channel_check()
{
    int8_t val;
    rf_polling_rx_on();
#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
#endif
    nrk_spin_wait_us(250);
    val=CCA_IS_1;
    //if(val) rf_rx_off(); 
#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time2);
    nrk_time_compact_nanos(&cur_radio_time2);
    nrk_time_sub(&cur_radio_time, cur_radio_time2, cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
    nrk_time_add(&radio_time, radio_time, cur_radio_time);
    nrk_time_compact_nanos(&radio_time);
#endif
    rf_rx_off();
    return val;
}
/**
 *  This is a callback if you require immediate response to a packet
 */

RF_RX_INFO *rf_rx_callback (RF_RX_INFO * pRRI)
{
    // Any code here gets called the instant a packet is received from the interrupt   
    return pRRI;
}

/*************
 * tdma_sync_status
 *
 * Checks whether this node is in sync
 * returns : 1 if sync'd, 0 otherwise
 * ***********/
uint8_t tdma_sync_status()
{
    return in_sync;
}
/*
int8_t tdma_encryption_set_ctr_counter(uint8_t *counter, uint8_t len)
{
if(len!=4 ) return NRK_ERROR;
rf_security_set_ctr_counter(counter);
   return NRK_OK;
}

int8_t tdma_tx_reserve_set( nrk_time_t *period, uint16_t pkts )
{

#ifdef NRK_MAX_RESERVES
// Create a reserve if it doesn't exist
if(tx_reserve==-1) tx_reserve=nrk_reserve_create();
if(tx_reserve>=0)
  return nrk_reserve_set(tx_reserve, period,pkts,NULL);
else return NRK_ERROR;
#else
return NRK_ERROR;
#endif
}

uint16_t tdma_tx_reserve_get()
{
#ifdef NRK_MAX_RESERVES
if(tx_reserve>=0)
  return nrk_reserve_get(tx_reserve);
else return 0;
#else
return 0;
#endif
}


int8_t  tdma_auto_ack_disable() 
{
rf_auto_ack_disable();
return NRK_OK;
}

int8_t  tdma_auto_ack_enable() 
{
rf_auto_ack_enable();
return NRK_OK;
}

int8_t  tdma_addr_decode_disable() 
{
rf_addr_decode_disable();
return NRK_OK;
}

int8_t  tdma_addr_decode_enable() 
{
rf_addr_decode_enable();
return NRK_OK;
}

int8_t tdma_addr_decode_set_my_mac(uint16_t my_mac)
{
rf_addr_decode_set_my_mac(my_mac);
return NRK_OK;
}

int8_t  tdma_addr_decode_dest_mac(uint16_t dest) 
{
tdma_rfTxInfo.destAddr=dest;
return NRK_OK;
}

int8_t tdma_rx_pkt_is_encrypted()
{
return rf_security_last_pkt_status();
}

int8_t tdma_encryption_set_key(uint8_t *key, uint8_t len)
{
  if(len!=16) return NRK_ERROR;
  rf_security_set_key(key);
  return NRK_OK;
}

int8_t tdma_encryption_enable()
{
  rf_security_enable();
  return NRK_OK;
}

int8_t tdma_encryption_disable()
{
  rf_security_disable();
  return NRK_OK;
}


int8_t tdma_set_rf_power(uint8_t power)
{
if(power>31) return NRK_ERROR;
rf_tx_power(power);
return NRK_OK;
}

void tdma_set_cca_active(uint8_t active)
{
cca_active=active;
}

int8_t tdma_set_cca_thresh(int8_t thresh)
{
  rf_set_cca_thresh(thresh); 
return NRK_OK;
}

int8_t tdma_set_channel(uint8_t chan)
{
if(chan>26) return NRK_ERROR;
g_chan=chan;
rf_init (&tdma_rfRxInfo, chan, 0xFFFF, 0x00000);
return NRK_OK;
}

void tdma_disable()
{
  is_enabled=0;
}

void tdma_enable()
{
  is_enabled=1;
  nrk_event_signal (tdma_enable_signal);
}


*/

int8_t tdma_rx_pkt_check()
{
    return tdma_rx_data_ready;
}

int8_t tdma_tx_pkt_check()
{
    return tdma_tx_data_ready; 
}

int8_t tdma_wait_until_rx_pkt()
{
nrk_sig_mask_t event;

if(tdma_rx_pkt_check()==1) return NRK_OK;

    nrk_signal_register(tdma_rx_pkt_signal); 
    event=nrk_event_wait (SIG(tdma_rx_pkt_signal));

// Check if it was a time out instead of packet RX signal
if((event & SIG(tdma_rx_pkt_signal)) == 0 ) return NRK_ERROR;
else return NRK_OK;
}

int8_t tdma_wait_until_rx_or_tx()
{
    nrk_signal_register(tdma_rx_pkt_signal);
    nrk_signal_register(tdma_tx_pkt_done_signal);
    nrk_event_wait (SIG(tdma_rx_pkt_signal) | SIG(tdma_tx_pkt_done_signal));
    return NRK_OK;

}

int8_t tdma_wait_until_tx()
{
    nrk_signal_register(tdma_tx_pkt_done_signal);
    nrk_event_wait (SIG(tdma_tx_pkt_done_signal));
    return NRK_OK;
}


int8_t tdma_rx_pkt_set_buffer(uint8_t *buf, uint8_t size)
{
if(buf==NULL) return NRK_ERROR;
    tdma_rfRxInfo.pPayload = buf;
    tdma_rfRxInfo.max_length = size;
    rx_buf_empty=1;
return NRK_OK;
}

nrk_time_t tdma_sync_time_get()
{
    return sync_time;
}


int8_t tdma_init (uint8_t chan)
{
    // wait for schedule if necessary before initializing TDMA 
    
    if (tdma_schedule_method == TDMA_SCHED_TREE)
    {
        while (!tdma_tree_made())
        {
#ifdef TDMA_TEXT_DEBUG
            nrk_kprintf(PSTR("Waiting for schedule to be made\r\n"));
#endif
            nrk_wait_until_next_period();
        }
    }

    sync_tolerance_time.secs = TDMA_SYNC_THRESHOLD_SECS;
    sync_tolerance_time.nano_secs = 0;

    tdma_rx_slot = 0;
    tdma_param.channel = chan;
    tdma_param.tx_guard_time = TX_GUARD_TIME_US;

    guard_time.secs = 0; // no way this could be more than 1sec
    guard_time.nano_secs = (uint32_t) TX_GUARD_TIME_US * 1000 + ((uint32_t)TREE_MAX_CHILDREN * TDMA_DELTA_SLOT_MS * NANOS_PER_MS);
    nrk_time_compact_nanos(&guard_time);
    //printf("GUARD TIME IS %20"PRIu32":%20"PRIu32"\r\n",
    //                     guard_time.secs, guard_time.nano_secs);

    sync_offset_time.secs = 0;
    sync_offset_time.nano_secs = 1 * NANOS_PER_MS;  // TX delay
    nrk_time_add(&sync_offset_time, sync_offset_time, guard_time);

    tdma_rx_data_ready = 0;
    tdma_tx_data_ready = 0;
    tdma_time_token = 0;

    timeout_duration.secs = 0;
    timeout_duration.nano_secs = (uint32_t) TDMA_RX_WAIT_MS * NANOS_PER_MS;
    nrk_time_compact_nanos(&timeout_duration);

#ifdef TDMA_RADIO_TIME_DEBUG
    radio_time.secs = 0;
    radio_time.nano_secs = 0;
    cur_radio_time.secs = 0;
    cur_radio_time.nano_secs = 0;
    cur_radio_time2.secs = 0;
    cur_radio_time2.nano_secs = 0;
#endif

#ifdef TDMA_AWAKE_TIME_DEBUG
    awake_time.secs = 0;
    awake_time.nano_secs = 0;
    awake_time1.secs = 0;
    awake_time1.nano_secs = 0;
    awake_time2.secs = 0;
    awake_time2.nano_secs = 0;
#endif

    read_eeprom_mac_address(&mac_address);
    
    my_addr16 = (mac_address & 0xFFFF);
    tdma_rx_pkt_signal=nrk_signal_create();
    if(tdma_rx_pkt_signal==NRK_ERROR)
    {
    nrk_kprintf(PSTR("TDMA ERROR: creating rx signal failed\r\n"));
    nrk_kernel_error_add(NRK_SIGNAL_CREATE_ERROR,nrk_cur_task_TCB->task_ID);
    return NRK_ERROR;
    }
    tdma_tx_pkt_done_signal=nrk_signal_create();
    if(tdma_tx_pkt_done_signal==NRK_ERROR)
    {
    nrk_kprintf(PSTR("TDMA ERROR: creating tx signal failed\r\n"));
    nrk_kernel_error_add(NRK_SIGNAL_CREATE_ERROR,nrk_cur_task_TCB->task_ID);
    return NRK_ERROR;
    }
    tdma_enable_signal=nrk_signal_create();
    if(tdma_enable_signal==NRK_ERROR)
    {
    nrk_kprintf(PSTR("TDMA ERROR: creating enable signal failed\r\n"));
    nrk_kernel_error_add(NRK_SIGNAL_CREATE_ERROR,nrk_cur_task_TCB->task_ID);
    return NRK_ERROR;
    }
    // Set the one main rx buffer
    tdma_rfRxInfo.pPayload = tdma_rx_buf;
    tdma_rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;

    // set the default tx buffer
    tdma_rfTxInfo.pPayload = tdma_tx_buf;
    tdma_rfTxInfo.length = TDMA_DATA_START;

    // Setup the cc2420 chip
    rf_init (&tdma_rfRxInfo, chan, 0xffff, my_addr16);
 
    srand(my_addr16);
    //FASTSPI_SETREG(CC2420_RSSI, 0xE580); // CCA THR=-25
    //FASTSPI_SETREG(CC2420_TXCTRL, 0x80FF); // TX TURNAROUND = 128 us
    //FASTSPI_SETREG(CC2420_RXCTRL1, 0x0A56); 
    // default cca thresh of -45
    rf_set_cca_thresh(TDMA_CCA_THRESH); 
#ifdef TDMA_TEXT_DEBUG
    printf("TDMA Running. I am %d.\r\n" , my_addr16);
#endif

    tdma_init_done = 1;
    return NRK_OK;
}

uint8_t tdma_initialized()
{
    return tdma_init_done;
}

int8_t tdma_mode_set(tdma_node_mode_t mode)
{
    tdma_node_mode = mode;
    return NRK_OK;
}

int8_t tdma_tx_pkt(uint8_t * buf, uint8_t len)
{
    if (tdma_tx_data_ready == 1)
        return NRK_ERROR;

    tdma_tx_data_ready = 1;
    tdma_rfTxInfo.pPayload = buf;
    tdma_rfTxInfo.length = len;
    
    return NRK_OK;

}


int8_t tdma_rx_pkt_release(void)
{
    tdma_rx_data_ready=0;
    return NRK_OK;

}


uint8_t* tdma_rx_pkt_get (uint8_t *len, int8_t *rssi,uint8_t *slot)
{
if(tdma_rx_pkt_check()==0)
    {
    *len=0;
    *rssi=0;
    *slot=0;
    return NULL;
    }
  *len=tdma_rfRxInfo.length;
  *rssi=tdma_rfRxInfo.rssi;
  *slot=tdma_rx_slot;

return tdma_rfRxInfo.pPayload;
}

int8_t tdma_addr_decode_set_my_mac(uint16_t my_mac)
{
    rf_addr_decode_set_my_mac(my_mac);
    return NRK_OK;
}



/**
 * _tdma_rx()
 *
 * This is the low level RX packet function.  It will read in
 * a packet and buffer it in the link layer's single RX buffer.
 * This buffer can be checked with rtl_check_rx_status() and 
 * released with rtl_release_rx_packet().  If the buffer has not
 * been released and a new packet arrives, the packet will be lost.
 * This function is only called from the timer interrupt routine.
 *
 * Arguments: slot is the current slot that is actively in RX mode.
 */
void _tdma_rx (uint16_t slot)
{
    uint8_t heard_sender, n, v;
    uint16_t tmp;

    //nrk_kprintf(PSTR("Calling tdma_rx()\r\n"));


    /*
    ** Slot stealing RX procedure
    ** -- LISTEN for delta slots
    ** --   AT DETECT, start to jam channel 
    ** -- RX at begin of real slot 
    */
    
    nrk_time_t data_slot_time;
    data_slot_time.secs = 0;
    data_slot_time.nano_secs = (uint32_t) TREE_MAX_CHILDREN * TDMA_DELTA_SLOT_MS * NANOS_PER_MS;
    nrk_time_compact_nanos(&data_slot_time);

    nrk_time_add(&data_slot_time, data_slot_time, time_of_slot);
    nrk_time_compact_nanos(&data_slot_time);

    nrk_time_get((nrk_time_t *) &current_time);

    heard_sender = 0;
    if (nrk_time_cmp(current_time, data_slot_time) > 0)
    {
        nrk_kprintf(("LATE for start of slot\r\n"));
    }
    else
    {
        #ifdef GPIO_DELTA_DEBUG
            nrk_gpio_set (NRK_DEBUG_1);
        #endif

        while(nrk_time_cmp(current_time, data_slot_time) < 0)
        {
            nrk_time_get(&current_time);
#ifdef SLOT_STEALING
            if (!heard_sender && !(v = _tdma_channel_check())) // if channel activity
            {
                //nrk_kprintf(PSTR("RX: Delta slot activity\r\n"));
                //printf("RX: Time is %20"PRIu32" %20"PRIu32"\r\n", current_time.secs, current_time.nano_secs);
                //rf_test_mode();
                heard_sender = 1;
                rf_test_mode();
                rf_carrier_on();
    #ifdef TDMA_RADIO_TIME_DEBUG
                nrk_time_get(&cur_radio_time);
                nrk_time_compact_nanos(&cur_radio_time);
    #endif

    #ifdef GPIO_DELTA_DEBUG
                nrk_gpio_clr (NRK_DEBUG_1);
                nrk_gpio_set (NRK_DEBUG_1);
    #endif
    #ifdef LED_DELTA_DEBUG
                nrk_led_set  (RED_LED);
    #endif

            }
#endif // if SLOT_STEALING
        }
        //printf("RX: DSTime is %20"PRIu32" %20"PRIu32"\r\n", data_slot_time.secs, data_slot_time.nano_secs);
    }

#ifdef SLOT_STEALING

    #ifdef TDMA_RADIO_TIME_DEBUG
        if (heard_sender)
        {
            nrk_time_get(&cur_radio_time2);
            nrk_time_compact_nanos(&cur_radio_time2);
            nrk_time_sub(&cur_radio_time, cur_radio_time2, cur_radio_time);
            nrk_time_compact_nanos(&cur_radio_time);
            nrk_time_add(&radio_time, radio_time, cur_radio_time);
            nrk_time_compact_nanos(&radio_time);
        }
    #endif
    #ifdef GPIO_DELTA_DEBUG
        nrk_gpio_clr (NRK_DEBUG_1);
    #endif
    #ifdef LED_DELTA_DEBUG
        nrk_led_clr  (RED_LED);
    #endif

        rf_carrier_off();
        rf_data_mode();

        if (!heard_sender)
        {
            // No one sent a signal; I'm not expecting a packet,
            // so I can finish and sleep.
            #ifdef TDMA_TEXT_DEBUG
                nrk_kprintf(PSTR("No sender heard\r\n"));
            #endif
            return;
        }

    #ifdef GPIO_BASIC_DEBUG
        nrk_gpio_set(NRK_DEBUG_1);
    #endif
    #ifdef LED_BASIC_DEBUG
        nrk_led_set(GREEN_LED);
    #endif

    // NEW: wait for a litle bit to make sure I don't catch some of my
    // own signal.
    nrk_spin_wait_us(500);

#endif // if SLOT_STEALING

    rf_flush_rx_fifo();
    rf_polling_rx_on ();
    
#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
#endif
    
    //timeout = _nrk_get_high_speed_timer();
    //timeout+=rtl_param.rx_timeout;
    // FIXME: Change back to high speed timer with overflow catching
    //timeout = _nrk_os_timer_get();
    //timeout+=4; //rtl_param.rx_timeout;
    

    nrk_time_get(&current_time);
    

    if (nrk_time_cmp(current_time,data_slot_time) > 0)
    {
        // current time is AFTER the data slot start.  We'll wait less.
        nrk_time_sub(&timeout_time, current_time, data_slot_time);
        nrk_time_compact_nanos(&timeout_time);
        //printf("Late by %20"PRIu32" %20"PRIu32"\r\n", timeout_time.secs, timeout_time.nano_secs);
        //    printf("curt %20"PRIu32" %20"PRIu32"\r\n", current_time.secs, current_time.nano_secs);
        //    printf("tos %20"PRIu32" %20"PRIu32"\r\n", time_of_slot.secs, time_of_slot.nano_secs);
        //nrk_kprintf(PSTR("Woke up late\r\n"));
    }

    nrk_time_add(&timeout_time, data_slot_time, timeout_duration);
    nrk_time_compact_nanos(&timeout_time);

    if (nrk_time_cmp(current_time, timeout_time) < 0)
    {
        nrk_time_t t2_time;
        nrk_time_sub(&t2_time, timeout_time, current_time);
        nrk_time_compact_nanos(&t2_time);
        //printf("I will wait for %20"PRIu32" %20"PRIu32"\r\n", t2_time.secs, t2_time.nano_secs);
    }
    else
    {
        #ifdef TDMA_TEXT_DEBUG
            nrk_kprintf(PSTR("REALLY FRICKIN' LATE FOR RX\r\n"));
        #endif
        //    printf("curt %20"PRIu32" %20"PRIu32"\r\n", current_time.secs, current_time.nano_secs);
        //    printf("curt %20"PRIu32" %20"PRIu32"\r\n", timeout_time.secs, timeout_time.nano_secs);
    }
    

    /*
    nrk_time_t time_from_slot_start;
    nrk_time_sub(&time_from_slot_start, current_time, slot_start_time);
    uint32_t tfss_us = (time_from_slot_start.secs * 1000000) + (time_from_slot_start.nano_secs / 1000);
    uint32_t curt_us = (current_time.secs * 1000000) + (current_time.nano_secs / 1000);

    uint32_t wait_time =tdma_param.tx_guard_time - tfss_us;
    printf("RX: spin waiting for %d us\r\n", wait_time);
    */

     n = 0;

     // I want to try fifop instead?
     // TODO: Ask which is better
     //
//    while ((n = rf_rx_check_sfd()) == 0) {
    //printf("TIME TO WAIT: %20"PRIu32" :  %20"PRIu32"\r\n",timeout_time.secs,timeout_time.nano_secs);


        while ((n = rf_rx_check_fifop()) == 0)
        {
            nrk_time_get(&current_time);

            //nrk_kprintf(PSTR("WAITING...\r\n"));
            //printf("CURTIME %20"PRIu32" :  %20"PRIu32"\r\n",current_time.secs,current_time.nano_secs);

            if (nrk_time_cmp(current_time, timeout_time) >= 0)
            {
                rf_rx_off ();
#ifdef GPIO_BASIC_DEBUG
                nrk_gpio_clr(NRK_DEBUG_1);
#endif
#ifdef LED_BASIC_DEBUG
                nrk_led_clr(GREEN_LED);
#endif

#ifdef TDMA_RADIO_TIME_DEBUG
                nrk_time_get(&cur_radio_time2);
                nrk_time_compact_nanos(&cur_radio_time2);
                nrk_time_sub(&cur_radio_time, cur_radio_time2, cur_radio_time);
                nrk_time_compact_nanos(&cur_radio_time);
                nrk_time_add(&radio_time, radio_time, cur_radio_time);
                nrk_time_compact_nanos(&radio_time);
#endif
                    // FIXME: add this back later
            // rtl_debug_dropped_pkt();
                //nrk_gpio_clr(NRK_DEBUG_1);
                //printf("Leaving tx  %20"PRIu32" :  %20"PRIu32"\r\n",current_time.secs,current_time.nano_secs);
                return;
            }
        }
        //nrk_time_get(&possible_sync_time);
        possible_sync_time = current_time;
        nrk_time_compact_nanos(&possible_sync_time);
     
#ifdef GPIO_BASIC_DEBUG
        nrk_gpio_clr(NRK_DEBUG_1);
#endif
        //timeout = _nrk_os_timer_get ();
        // This is important!
        // If a preamble is decoded, but the packet fails,
        // you must terminate before the end of the slot to avoid a timming error...
        //timeout += 30;  // was 30

        //timeout = _nrk_os_timer_get(); 
        //timeout += 5;               // was 30
        
        nrk_time_get(&current_time);
        twait_duration.secs = 0;
        twait_duration.nano_secs = TDMA_SLOT_TIME_MS * NANOS_PER_MS;
        nrk_time_add(&twait_duration, current_time, twait_duration);

#ifdef GPIO_BASIC_DEBUG
        nrk_gpio_set(NRK_DEBUG_1);
#endif

        if (n != 0) 
        {
            n = 0;
            // Packet on its way
            //nrk_kprintf(PSTR("Packet arriving!\r\n"));
            while ((n = rf_polling_rx_packet ()) == 0) 
            {
                nrk_time_get(&current_time);
                if (nrk_time_cmp(current_time, twait_duration) >= 0)
                {
                    #ifdef TDMA_TEXT_DEBUG
                        nrk_kprintf( PSTR("pkt timed out\r\n") );
                    #endif
                    break;          // huge timeout as failsafe
                }
            }
        } 

#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time2);
    nrk_time_compact_nanos(&cur_radio_time2);
    nrk_time_sub(&cur_radio_time, cur_radio_time2, cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
    nrk_time_add(&radio_time, radio_time, cur_radio_time);
    nrk_time_compact_nanos(&radio_time);
#endif


    rf_rx_off ();

#ifdef GPIO_BASIC_DEBUG
    nrk_gpio_clr(NRK_DEBUG_1);
#endif

    if (n == 1)
    {

        // Added for debugging
#ifdef DEBUG_RSSI
        if (tdma_rfRxInfo.rssi < 0)
        {
            printf("RSSI low %d\r\n", tdma_rfRxInfo.rssi);
            // right now; leave GREEN_LED active

            return;
        }
#endif
            // CRC and checksum passed
        // FIXME: DEBUG add back
        //rtl_debug_rx_pkt(1);

        uint8_t tmp_token = tdma_rfRxInfo.pPayload[TDMA_TIME_TOKEN] & 0x7F;
        //printf("Received token %d after AND %d, my token: %d\r\n", 
        //tdma_rfRxInfo.pPayload[TDMA_TIME_TOKEN], tmp_token, tdma_time_token);

        // check the received time token against current one
        // if it's newer, sync to it.
        // token loops around, so allow some slack
        if (tdma_node_mode != TDMA_MASTER && (tmp_token > tdma_time_token || 
                    (tdma_time_token > 110 && tmp_token < 10 )))
        {   
            //we have a valid packet, so sync to it
            nrk_time_sub(&sync_time, possible_sync_time, sync_offset_time);
            nrk_time_compact_nanos(&sync_time);
            //sync_time = possible_sync_time;
            //tdma_rx_slot = slot;
            tmp = tdma_rfRxInfo.pPayload[TDMA_SLOT];
            tmp <<= 8;
            tmp |= tdma_rfRxInfo.pPayload[TDMA_SLOT + 1];

            //timeout_duration.nano_secs -= (NANOS_PER_MS / 10);

#ifdef TDMA_SYNC_DEBUG
            resync_count++;
            //printf("<<<<<<<<<<<<<ON THE FLY SYNC>>>>>>>>>>>>>>>\r\n");
            printf("RSYNC %d %d %20"PRIu32" %20"PRIu32"\r\n", 
                    my_addr16, sync_count, sync_time.secs, sync_time.nano_secs);
#endif

            //printf("Changing timeout to %20"PRIu32" ns\r\n", timeout_duration.nano_secs);
            //printf("Slot to sync = %d\r\n", tmp);
            nrk_led_toggle(ORANGE_LED);
            sync_slot = tmp;

            //update my token
            tdma_time_token = tmp_token;

        }
        else
        {
            //nrk_kprintf(PSTR("Not syncing, time token is old\r\n"));
        }
    /*
        if(tmp!=sync_slot)
        {
            // XXX HUGE HACK!
            // This shouldn't happen, but it does.  This should
            // be fixed soon.
            printf( "global slot mismatch: %d %d\r\n",global_slot,tmp );
            sync_slot=tmp;
        }   
    */

        //printf ("my slot = %d  rx slot = %d\r\n", global_slot, tmp);
    
        //if (rx_callback != NULL)
            //rx_callback (slot);
      // check if packet is an explicit time sync packet
        if((tdma_rfRxInfo.pPayload[TDMA_TIME_TOKEN]&0x80)==0)
        {
            // if we got a good packet, send the signal to
            // the application.  Shouldn't need to check rx
            // mask here since this should only get called by real
            // rx slot.
            tdma_rx_data_ready = 1;
            nrk_event_signal (tdma_rx_pkt_signal);
            tdma_rx_slot = slot;
        }
        else
        { 
            // If it is an explicit time sync packet, then release it
            // so it doesn't block a buffer...
            //nrk_kprintf( PSTR("got explicit sync\r\n") );
            tdma_rx_pkt_release(); 
        }
    } 
#ifdef TDMA_TEXT_DEBUG
    else 
        printf( "RX: Error = %d\r\n",(int8_t)n );           
#endif

#ifdef LED_BASIC_DEBUG
    nrk_led_clr (GREEN_LED);
#endif

} // END TDMA_RX

/**
 * _tdma_tx()
 *
 * This function is the low level TX function.
 * It is only called from the timer interrupt and fetches any
 * packets that were set for a particular slot by rtl_tx_packet().
 *
 * Arguments: slot is the active slot set by the interrupt timer.
 */
void _tdma_tx (uint16_t slot)
{
    int8_t explicit_tsync;
    uint8_t success;
    uint32_t g;

    if (tdma_sync_status () == 0)
        return;                 // don't tx if you aren't sure you are in sync
    //if (tx_callback != NULL)
     ////   tx_callback (slot);
    // Copy the element from the smaller vector of TX packets
    // to the main TX packet

    
    /*
    tdma_rfTxInfo.pPayload=tdma_tx_info[slot].pPayload;
    tdma_rfTxInfo.length=tdma_tx_info[slot].length;
    */

    //tx_buf[TDMA_DATA_START] = '\0';
    //tdma_rfTxInfo.pPayload=tx_buf;
    //tdma_rfTxInfo.length=TDMA_DATA_START;

    //printf("Encoding global slot %d\r\n", slot);
    tdma_rfTxInfo.pPayload[TDMA_SLOT] = (-1 >> 8);
    tdma_rfTxInfo.pPayload[TDMA_SLOT + 1] = (-1 & 0xFF);
    // or in so that you don't kill 
    //tdma_rfTxInfo.pPayload[TDMA_TIME_TOKEN]|= _rtl_time_token;  

    // This clears the explicit sync bit
    tdma_rfTxInfo.pPayload[TDMA_TIME_TOKEN]= -1; 
    explicit_tsync=0;
    // If it is an empty packet set explicit sync bit
    //printf("Length of info struct: %d\r\n", tdma_rfTxInfo.length);
    if(tdma_rfTxInfo.length==TDMA_DATA_START )
    {
        explicit_tsync=1;
        tdma_rfTxInfo.pPayload[TDMA_TIME_TOKEN]|= 0x80;
    } 
    else
    {
       // printf("Sending packet content: %s\r\n", &tx_buf[TDMA_DATA_START]);

    }
     
                                    // MSB (explicit time slot flag)

    //rf_carrier_off();
    //rf_data_mode();
    // send packet
    //rf_rx_off();

    /*
    ** The slot stealing procedure is this...
    ** -- SENSE the channel for the number of delta slots equal to my priority
    ** -- CHECK if I have detected activity.  If yes, then call the whole thing off.
    ** -- FLOOD the channel for the remainder of delta slots.
    ** -- WAIT the standard TX Guard Time
    ** -- TRANSMIT my packet
    */

#ifdef SLOT_STEALING
    nrk_time_get(&current_time);
    nrk_time_compact_nanos(&current_time);

    // set a time to start with my delta slot
    nrk_time_t delta_offset_time;
    nrk_time_t next_delta_offset_time;

    // delta offset -- The time of the start of MY delta slot
    delta_offset_time.secs = 0;
    delta_offset_time.nano_secs = (uint32_t) TDMA_DELTA_SLOT_MS * NANOS_PER_MS * next_schedule_entry.priority;
    
    nrk_time_compact_nanos(&delta_offset_time);
    nrk_time_add(&delta_offset_time, delta_offset_time, time_of_slot);
    nrk_time_compact_nanos(&delta_offset_time);

    // Next delta offset -- The time of the end of my delta slot (beginning of next)
    next_delta_offset_time.secs = 0;
    next_delta_offset_time.nano_secs = (uint32_t) TDMA_DELTA_SLOT_MS * NANOS_PER_MS;
    nrk_time_compact_nanos(&next_delta_offset_time);
    nrk_time_add(&next_delta_offset_time, next_delta_offset_time, delta_offset_time);
    nrk_time_compact_nanos(&next_delta_offset_time);

    //nrk_kprintf(PSTR("About to wait\r\n"));
    
    // If it's currently past my delta slot already, I can't transmit.
    if (nrk_time_cmp(current_time, next_delta_offset_time) > 0)
    {
        // I'm late!  Forget it
        #ifdef TDMA_TEXT_DEBUG
            nrk_kprintf(PSTR("Woke up late. Can't transmit.\r\n"));
        #endif
        rf_rx_off();
        return;
    }
    
    // if I'm a potential stealer, I need to sense the channel to see if it's
    // busy, meaning either the sender or receiver is jamming it.
    // Right now, if it's past the start of my delta slot, just checks once.
    if (next_schedule_entry.priority > 0)
    {
        g = 0;
        do
        {
            nrk_time_get(&current_time);
            g++;
            //nrk_kprintf(PSTR("Checking Channel\r\n"));
            if (!_tdma_channel_check()) // if channel activity
            {
                // Detected channel activity
                nrk_kprintf(PSTR("NO STEAL\r\n"));
                rf_rx_off();
                nrk_led_set(GREEN_LED);
                return;
            }
        } while (nrk_time_cmp(current_time, delta_offset_time) < 0);
        //printf("TX: did %d checks\r\n", g);
    }

    
    //nrk_kprintf(PSTR("Finish\r\n"));

    // done checking channel
    rf_rx_off();

#ifdef GPIO_DELTA_DEBUG
    nrk_gpio_set (NRK_DEBUG_1);
#endif

#ifdef LED_DELTA_DEBUG
    nrk_led_set (RED_LED);
#endif
    // do the jamming
    rf_test_mode();
    //rf_data_mode();
    rf_carrier_on();

#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
#endif


    g = 0;
    // jam for my entire delta slot, no longer
    while (nrk_time_cmp(current_time, next_delta_offset_time) < 0)
    {
        nrk_time_get(&current_time);
        g++;
    }

#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time2);
    nrk_time_compact_nanos(&cur_radio_time2);
    nrk_time_sub(&cur_radio_time, cur_radio_time2, cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
    nrk_time_add(&radio_time, radio_time, cur_radio_time);
    nrk_time_compact_nanos(&radio_time);
#endif
    // stop jamming
    rf_carrier_off();
    rf_test_mode();


#ifdef GPIO_DELTA_DEBUG
    nrk_gpio_clr (NRK_DEBUG_1);
#endif

#ifdef LED_DELTA_DEBUG
    nrk_led_clr (RED_LED);
#endif

    //printf("Jammed %d iters\r\n", g);

#endif // if SLOT_STEALING

    // data start time is the end of all the delta slots
    nrk_time_t data_start_time;
    data_start_time.secs = 0;
    data_start_time.nano_secs = (uint32_t) TREE_MAX_CHILDREN * TDMA_DELTA_SLOT_MS * NANOS_PER_MS;

    nrk_time_add(&data_start_time, data_start_time, time_of_slot);
    nrk_time_compact_nanos(&data_start_time);

    // wait until the start of the "data" portion of the slot
    do
    {
        nrk_time_get(&current_time);
    }
    while (nrk_time_cmp(current_time, data_start_time) < 0);

    // wait until tx time.  I'm not using tx_tdma_packet because it uses the high
    // speed timer, which I'm pretty sure I don't want to use.

    //printf("tos %20"PRIu32" %20"PRIu32"\r\ncurrent %20"PRIu32" %20"PRIu32"\r\n", 
    //        time_of_slot.secs, time_of_slot.nano_secs,
    //        current_time.secs, current_time.nano_secs);

    uint32_t wait_time_us;


    nrk_time_get(&current_time);
    nrk_time_compact_nanos(&current_time);

    //uint32_t curt_us = (current_time.secs * (uint32_t) 1000000) + (current_time.nano_secs / (uint32_t)1000);

    if (nrk_time_cmp(current_time, data_start_time) > 0)  // if we woke up after the appointed wake up time
    {
        nrk_time_t time_from_slot_start;
        nrk_time_sub(&time_from_slot_start, current_time, data_start_time);
        nrk_time_compact_nanos(&time_from_slot_start);
        //printf("CURRENT TIME         %20"PRIu32" %20"PRIu32"\r\n", current_time.secs, current_time.nano_secs);
        //printf("TIME OF SLOT START   %20"PRIu32" %20"PRIu32"\r\n", time_of_slot.secs, time_of_slot.nano_secs);
        //printf("TIME FROM SLOT START %20"PRIu32" %20"PRIu32"\r\n", time_from_slot_start.secs, time_from_slot_start.nano_secs);
        uint32_t tfss_us = (time_from_slot_start.secs * (uint32_t) 1000000) + (time_from_slot_start.nano_secs / (uint32_t) 1000);

        if (tfss_us > tdma_param.tx_guard_time)
        {
            nrk_kprintf(PSTR("LATE\r\n"));
            wait_time_us = 0;
        }
        else
        {
            wait_time_us = tdma_param.tx_guard_time - tfss_us;
        }
    }
    else  // I woke up before the slot
    {
        nrk_time_t time_until_slot_start;
        nrk_time_sub(&time_until_slot_start, data_start_time, current_time);
        nrk_time_compact_nanos(&time_until_slot_start);

        uint32_t tuss_us = (time_until_slot_start.secs * (uint32_t) 1000000)
                 + (time_until_slot_start.nano_secs / (uint32_t) 1000);
        wait_time_us = tdma_param.tx_guard_time + tuss_us;
        
    } 
    //printf("TFSS CURT %20"PRIu32" %20"PRIu32"\r\n", tfss_us, curt_us);

    nrk_spin_wait_us(wait_time_us); 

#ifdef GPIO_BASIC_DEBUG
    nrk_gpio_set (NRK_DEBUG_1);
#endif

#ifdef LED_BASIC_DEBUG
    nrk_led_set(RED_LED);
#endif

    // Don't fill packet til here
    tdma_rfTxInfo.pPayload[TDMA_SLOT] = (slot >> 8);
    tdma_rfTxInfo.pPayload[TDMA_SLOT + 1] = (slot & 0xFF);
    //tdma_rfTxInfo.pPayload[TDMA_SLOT] = (-1 >> 8);
    //tdma_rfTxInfo.pPayload[TDMA_SLOT + 1] = (-1 & 0xFF);
    // or in so that you don't kill 
    //tdma_rfTxInfo.pPayload[TDMA_TIME_TOKEN]|= _rtl_time_token;  

    // This clears the explicit sync bit
    tdma_rfTxInfo.pPayload[TDMA_TIME_TOKEN]= tdma_time_token; 

#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
#endif
    rf_data_mode();
    success = rf_tx_packet (&tdma_rfTxInfo);
    rf_rx_off();

#ifdef TDMA_RADIO_TIME_DEBUG
    nrk_time_get(&cur_radio_time2);
    nrk_time_compact_nanos(&cur_radio_time2);
    nrk_time_sub(&cur_radio_time, cur_radio_time2, cur_radio_time);
    nrk_time_compact_nanos(&cur_radio_time);
    nrk_time_add(&radio_time, radio_time, cur_radio_time);
    nrk_time_compact_nanos(&radio_time);
#endif

#ifdef GPIO_BASIC_DEBUG
    nrk_gpio_clr (NRK_DEBUG_1);
#endif
    //printf("TX: waited %20"PRIu32" us gt %d\r\n", wait_time_us, tdma_param.tx_guard_time);
    //rf_tx_tdma_packet (&tdma_rfTxInfo,slot_start_time, tdma_param.tx_guard_time);
    //tdma_tx_data_ready &= ~((uint32_t) 1 << slot);       // clear the flag

    if (success == FALSE)
        nrk_kprintf(PSTR("Bad TX\r\n"));

    if (explicit_tsync==0 && success == TRUE)
    {
        // if not a sync packet sent, notify that data was sent
        // and be ready to accept more
        tdma_tx_data_ready = 0;
        nrk_event_signal (tdma_tx_pkt_done_signal);
    }
    //if (slot >= (TDMA_FRAME_SLOTS - _rtl_contention_slots))
     //   _rtl_contention_pending = 0;
    // clear time sync token so that explicit time slot flag is cleared 
    //tdma_rfTxInfo.pPayload[TDMA_TIME_TOKEN]=0;  

    // useful, just not implementing yet (John)
    //if(explicit_tsync==0)
        //nrk_event_signal (tdma_tx_done_signal);

#ifdef LED_BASIC_DEBUG
    nrk_led_clr (RED_LED);
#endif
    
    // signal packet sent
    //nrk_time_get(&current_time);
    //        printf("In TX: Current time  %20"PRIu32" :  %20"PRIu32"\r\n",current_time.secs,current_time.nano_secs);



}  // END TDMA_TX


nrk_time_t get_time_of_next_wakeup(nrk_time_t current_time, uint16_t next_slot,
                                   uint16_t sync_slot, nrk_time_t sync_time)
{
    nrk_time_t time_since_sync;
    nrk_time_t time_until_wakeup;
    nrk_time_t next_wakeup_time;
    uint16_t slots_since_sync;
    uint16_t slot_in_cycle;
    uint32_t time_until_wakeup_ms;
    uint32_t time_since_sync_ms;

    if (!nrk_time_sub(&time_since_sync, current_time, sync_time))
    {
        nrk_kprintf(PSTR("FAILURE! Current time BEFORE SYNC TIME.\r\n  "));
        printf("cur %20"PRIu32" %20"PRIu32", st %20"PRIu32" %20"PRIu32"\r\n", current_time.secs, current_time.nano_secs, sync_time.secs, sync_time.nano_secs);
    }

    nrk_time_compact_nanos(&time_since_sync); //NEED THIS, IN A LOT OF PLACES...

    time_since_sync_ms = time_since_sync.secs * 1000 +
                                  (time_since_sync.nano_secs / (uint32_t) NANOS_PER_MS); 

    //printf("Time since sync_ms: %20"PRIu32"\r\n", time_since_sync_ms);

    slots_since_sync = ( time_since_sync_ms / (TDMA_SLOT_TIME_MS));
    slot_in_cycle = (slots_since_sync + sync_slot) % TDMA_SLOTS_PER_CYCLE;

    //printf("SLOT IN CYCLE: %d %d\r\n", slots_since_sync, slot_in_cycle);
    //printf("SLOT IN CYCLE: %20"PRIu32" %d %d\r\n", time_since_sync_ms, TDMA_SLOT_TIME_MS, slots_since_sync);

    if (slot_in_cycle < next_slot)
        time_until_wakeup_ms = (slots_since_sync + (next_slot-slot_in_cycle)) * (uint32_t)TDMA_SLOT_TIME_MS;
    else
        time_until_wakeup_ms = (slots_since_sync + TDMA_SLOTS_PER_CYCLE - slot_in_cycle + next_slot)
                                * (uint32_t) TDMA_SLOT_TIME_MS;

    //printf("MSEC to wakeup : %20"PRIu32"\r\n", time_until_wakeup_ms);
    // create a nrk_time_t to represent the time difference between now and next wakeup
    time_until_wakeup.secs      = time_until_wakeup_ms / 1000;
    time_until_wakeup.nano_secs = (time_until_wakeup_ms % 1000) * NANOS_PER_MS;
    nrk_time_compact_nanos(&time_until_wakeup);

    nrk_time_add(&next_wakeup_time, sync_time, time_until_wakeup);
         
    //printf("Next wakeup time (inside function): %20"PRIu32", %20"PRIu32"\r\n", next_wakeup_time.secs, next_wakeup_time.nano_secs);
    return next_wakeup_time;
}

int8_t tdma_schedule_method_set(tdma_sched_method_t m)
{
    tdma_schedule_method = m;
    return NRK_OK;
}


/*************************************
 * tdma_nw_task
 * ************************************
 */
void tdma_nw_task ()
{

if (tdma_schedule_method == TDMA_SCHED_TREE)
{
    tdma_schedule_tree_make(15);
}

// wait until tdma_init is done being called
do
{
    nrk_wait_until_next_period();
#ifdef TDMA_TEXT_DEBUG
    nrk_kprintf(PSTR("Waiting for tdma_init()\r\n"));
#endif
}
while (!tdma_init_done);

tdma_running=1;

if (tdma_node_mode == TDMA_MASTER)
{
#ifdef TDMA_MASTER_MODE
    in_sync = 1;
    next_schedule_entry = tdma_schedule_get_next(0);
    //printf("Schedule entry slot: %d\r\n", next_schedule_entry.slot);

    //transmit first sync immediately... sort of

    nrk_time_get(&time_of_slot);

    // Adding a delay to make sure the first TX is on time 
    time_of_slot.nano_secs += 50 * NANOS_PER_MS;
    nrk_time_compact_nanos(&time_of_slot);

    //sync_slot = next_schedule_entry.slot;
    sync_slot = 0;
    nrk_time_get(&sync_time);
#endif
}


#ifdef TDMA_AWAKE_TIME_DEBUG
    nrk_time_get(&awake_time1);
    nrk_time_compact_nanos(&awake_time1);
#endif

// set the RX buffer to use for TDMA
rf_set_rx (&tdma_rfRxInfo, tdma_param.channel);

#ifdef TDMA_TEXT_DEBUG
    printf("Begin\r\n");
#endif
//nrk_time_get(&current_time);
//printf("Current time on startup: %20"PRIu32"  %20"PRIu32" \r\n",current_time.secs, current_time.nano_secs);
 

    while (1)
    {
        if (tdma_node_mode == TDMA_SLAVE)
        {
            while (!in_sync)
            {
                //nrk_kprintf(PSTR("Waiting for sync\r\n"));
                nrk_led_clr(BLUE_LED);
                nrk_led_set(RED_LED);

                //poll for incoming packet
                tdma_rfRxInfo.pPayload[TDMA_SLOT] = (-1 >> 8);
                tdma_rfRxInfo.pPayload[TDMA_SLOT + 1] = (-1 & 0xFF);

                sync_slot = tdma_rfRxInfo.pPayload[TDMA_SLOT];
                sync_slot <<=8;
                sync_slot |= tdma_rfRxInfo.pPayload[TDMA_SLOT + 1];

                //printf("sync slot: %d\r\n", sync_slot);

                tdma_rfRxInfo.max_length = RF_MAX_PAYLOAD_SIZE;
                rf_polling_rx_on();

                sync_time.secs = 0;
                sync_time.nano_secs = 0;
                
                do 
                {
                    nrk_time_get(&sync_time);
                }
                while (rf_rx_check_fifop() == 0);

                if (sync_time.secs == 0 && sync_time.nano_secs == 0)
                    nrk_kprintf(PSTR("SYNC TIME IS 0. THERE'S A PROBLEM\r\n"));
#ifdef GPIO_SYNC_DEBUG
                nrk_gpio_set(NRK_DEBUG_1);
                nrk_gpio_set(NRK_DEBUG_0);
#endif

                nrk_time_compact_nanos(&sync_time);
                nrk_time_sub(&sync_time, sync_time, sync_offset_time);
                nrk_time_compact_nanos(&sync_time);

                //packet coming
                while ((n = rf_polling_rx_packet()) == 0)
                {
                }

                rf_rx_off();

                if (n != 1)
                {
                    printf("FAILED recv: n = %d\r\n", n);
                    continue;
                }

#ifdef GPIO_SYNC_DEBUG
                nrk_gpio_clr(NRK_DEBUG_1);
#endif
                
        // Added for debugging
#ifdef DEBUG_RSSI
        if (tdma_rfRxInfo.rssi < 0)
        {
            printf("RSSI low %d\r\n", tree_rfRxInfo.rssi);
            continue;
        }
#endif

                /*printf( "TDMA: SNR= %d [",tdma_rfRxInfo.rssi );
                for(uint8_t i=0; i<tdma_rfRxInfo.length; i++ )
                        printf( "%c", tdma_rfRxInfo.pPayload[i]);
                printf( "]\r\n" );
                */

                //read the slot we're going to assign
                //printf("Recv : %s\r\n", tdma_rfRxInfo.pPayload);
                //sscanf(tdma_rfRxInfo.pPayload,"%16"PRIx32"", &sync_slot);
                
                sync_slot = tdma_rfRxInfo.pPayload[TDMA_SLOT];
                sync_slot <<=8;
                sync_slot |= tdma_rfRxInfo.pPayload[TDMA_SLOT + 1];

                // I need to make sure this packet is real, and not just noise caused by
                // the master. This will probably happen the first time I see a sync packet

                
                tdma_time_token = 0x7F & tdma_rfRxInfo.pPayload[TDMA_TIME_TOKEN];
                //sscanf(tdma_rfRxInfo.pPayload,"%d", &sync_slot);
                tdma_rx_pkt_release();

                //printf("Sync slot = %d\r\n", sync_slot);
                //printf("Token = %d\r\n", tdma_time_token);

                if (sync_slot < 0 || sync_slot > TDMA_SLOTS_PER_CYCLE)
                {
                    //nrk_kprintf(PSTR("INVALID SYNC\r\n"));
                    continue;
                }

#ifdef TDMA_SYNC_DEBUG
                sync_count++;
                printf("SYNC %d %d %20"PRIu32" %20"PRIu32" %d\r\n",
                         my_addr16, sync_count, 
                         sync_time.secs, sync_time.nano_secs, sync_slot);
#endif
                
                nrk_led_clr(RED_LED);
                in_sync = 1;
#ifdef GPIO_SYNC_DEBUG
                nrk_gpio_clr(NRK_DEBUG_0);
                //in_sync = 0;
#endif

                //next_schedule_entry = tdma_schedule_get_next(sync_slot);
                next_schedule_entry = tdma_schedule_get_next(sync_slot);

#ifdef GPIO_SLOT_DEBUG
                nrk_gpio_set(NRK_DEBUG_0);
#endif

            } //while not in sync

            nrk_led_set(BLUE_LED);
            

            //printf("sync slot is %d, next wakeup on slot %d\r\n", sync_slot, next_schedule_entry.slot);

            nrk_time_get(&current_time);
            nrk_time_compact_nanos(&current_time);
            
            //printf("Sync time: %20"PRIu32" : %20"PRIu32", Current time  %20"PRIu32" :  %20"PRIu32"\r\n",
                    //sync_time.secs, sync_time.nano_secs,
                    //current_time.secs,current_time.nano_secs);

            nrk_time_sub(&time_since_sync, current_time, sync_time);
            nrk_time_compact_nanos(&time_since_sync);
    
           /* printf("Time since sync: %20"PRIu32" : %20"PRIu32", tolerance  %20"PRIu32" : %20"PRIu32"\r\n",
                    time_since_sync.secs, time_since_sync.nano_secs,
                    sync_tolerance_time.secs, sync_tolerance_time.nano_secs);
                    */
            //printf("tss: %20"PRIu32",%20"PRIu32"."
            //printf("sync tol: %20"PRIu32",%20"PRIu32"\r\n", 
            //        sync_tolerance_time.secs, sync_tolerance_time.nano_secs );

            //printf("Time since sync, %20"PRIu32", %20"PRIu32"\r\n", time_since_sync.secs,
            //                                        time_since_sync.nano_secs);
            if (nrk_time_cmp(time_since_sync, sync_tolerance_time) > 0)
            {
#ifdef TDMA_SYNC_DEBUG
                oos_count++;
                nrk_time_get(&current_time);
                nrk_time_compact_nanos(&current_time);
                printf("OOS %d %d %20"PRIu32" %20"PRIu32"\r\n",
                        my_addr16, oos_count,
                        current_time.secs, current_time.nano_secs);

#endif
                // sync time has expired.  Go into re-sync mode
                in_sync = 0;
                tdma_time_token = 0;
                continue;
            }

        }


#ifdef TDMA_AWAKE_TIME_DEBUG
        nrk_time_get(&awake_time2);
        nrk_time_compact_nanos(&awake_time2);
        nrk_time_sub(&awake_time2, awake_time2, awake_time1);
        nrk_time_compact_nanos(&awake_time2);
        nrk_time_add(&awake_time, awake_time, awake_time2);
        nrk_time_compact_nanos(&awake_time);

        printf("AWK %d %20"PRIu32" %20"PRIu32" %20"PRIu32" %20"PRIu32"\r\n",
            my_addr16, current_time.secs, current_time.nano_secs,
            awake_time.secs, awake_time.nano_secs);
#endif
#ifdef TDMA_RADIO_TIME_DEBUG
        printf("RDO %d %20"PRIu32" %20"PRIu32" %20"PRIu32" %20"PRIu32"\r\n",
            my_addr16, current_time.secs, current_time.nano_secs,
            radio_time.secs, radio_time.nano_secs);
#endif


        nrk_time_get(&current_time);
        nrk_time_compact_nanos(&current_time);

        time_of_slot = get_time_of_next_wakeup(current_time, 
                                    next_schedule_entry.slot, sync_slot, sync_time);
        nrk_time_compact_nanos(&time_of_slot);

#ifdef GPIO_SLOT_DEBUG
        nrk_gpio_clr(NRK_DEBUG_0);
#endif

        //nrk_led_clr(BLUE_LED);
        nrk_wait_until(time_of_slot);
        //nrk_led_set(BLUE_LED);

#ifdef GPIO_SLOT_DEBUG
        nrk_gpio_set(NRK_DEBUG_0);
#endif
#ifdef TDMA_AWAKE_TIME_DEBUG
        nrk_time_get(&awake_time1);
        nrk_time_compact_nanos(&awake_time1);
#endif

        //nrk_time_get(&time_of_slot);

        //nrk_time_get(&current_time);
        //printf("Woke up at    %20"PRIu32" %20"PRIu32"\r\n", current_time.secs, current_time.nano_secs);

        nrk_led_clr(BLUE_LED);
        //printf("Done sleeping\r\n");

        if (next_schedule_entry.type == TDMA_RX) // RX
        {
            _tdma_rx(next_schedule_entry.slot);

            if (tdma_rx_pkt_check() == 0)
            {
                if (next_schedule_entry.priority == 1)
                {
                    // I was supposed to get a sync packet
#ifdef TDMA_SYNC_DEBUG
                    printf("NOSYNC %d\r\n",my_addr16);
#endif
                    
                }
                else
                {
#ifdef TDMA_TEXT_DEBUG
                    nrk_kprintf(PSTR("NO RX\r\n"));
#endif
                }

            }
            /*
            if (tdma_rx_pkt_check() != 0)
                printf("Received: %s\r\n", &tdma_rfRxInfo.pPayload[TDMA_DATA_START]);
            tdma_rx_pkt_release();
            */
        }
        else if (tdma_node_mode == TDMA_MASTER && (next_schedule_entry.type == TDMA_TX_CHILD))
        {
            // time to send a sync packet
           // printf("Master send, time token is %d, slot is %d\r\n", tdma_time_token, next_schedule_entry.slot);

            
            tdma_tx_buf[TDMA_DATA_START] = '\0';
            
            tdma_rfTxInfo.pPayload = tdma_tx_buf;
            tdma_rfTxInfo.length = TDMA_DATA_START;

            //nrk_time_get(&current_time);


            //printf("TIME1: %20"PRIu32":%20"PRIu32"\r\n", current_time.secs, current_time.nano_secs);
            //slot_start_time = current_time;
        

            // This needs to be BEFORE the TX! Otherwise it becomes after the guard time and
            // screws everything up!!
            // also, make it the time of the slot so that it doesn't offset by some amount either.
            sync_time = time_of_slot;
    
            //nrk_time_get(&sync_time);
            //nrk_time_compact_nanos(&sync_time);
            //sync_time = current_time;

            _tdma_tx(next_schedule_entry.slot);

            sync_slot = next_schedule_entry.slot;


            tdma_time_token++;
            if (tdma_time_token > 127)
                tdma_time_token = 0;

        }
        else if (tdma_node_mode == TDMA_SLAVE)
        {
            if (next_schedule_entry.type == TDMA_TX_CHILD)
            {
                // send a sync packet (use the existing buffer but do not modify it)
                uint8_t tmp_length = tdma_rfTxInfo.length; 
                tdma_rfTxInfo.length = TDMA_DATA_START;
                _tdma_tx(next_schedule_entry.slot); // send sync
                tdma_rfTxInfo.length = tmp_length;
#ifdef TDMA_TEXT_DEBUG
                nrk_kprintf(PSTR("Sent sync packet\r\n"));
#endif
            }
            else if (next_schedule_entry.type==TDMA_TX_PARENT)
            {
                if (tdma_tx_data_ready)
                {
                    // send a real packet
                    _tdma_tx(next_schedule_entry.slot);
#ifdef TDMA_TEXT_DEBUG
                    if (!tdma_tx_data_ready)
                    {
                        if (next_schedule_entry.priority == 0)
                            nrk_kprintf(PSTR("Normal send\r\n"));
                        else 
                            nrk_kprintf(PSTR("DIDSTEAL\r\n"));
                        //printf("Sent packet on slot %d\r\n", next_schedule_entry.slot);
                    }
#endif
                }
#ifdef TDMA_TEXT_DEBUG
                else
                    nrk_kprintf(PSTR("Nothing to send\r\n"));
#endif
            }
#ifdef TDMA_TEXT_DEBUG
            else
                nrk_kprintf(PSTR("Slave and invalid slot type\r\n"));
#endif
        }
#ifdef TDMA_TEXT_DEBUG
        else
            nrk_kprintf(PSTR("Master and invalid slot type\r\n"));
#endif

        next_schedule_entry = tdma_schedule_get_next(next_schedule_entry.slot);
    }
}



int8_t tdma_started()
{
    return tdma_running;
}

void tdma_task_config ()
{
    nrk_task_set_entry_function( &tdma_task, tdma_nw_task);
    nrk_task_set_stk( &tdma_task, tdma_task_stack, TDMA_STACK_SIZE);
    tdma_task.prio = TDMA_TASK_PRIORITY;
    tdma_task.FirstActivation = TRUE;
    tdma_task.Type = BASIC_TASK;
    tdma_task.SchType = PREEMPTIVE;
    tdma_task.period.secs = 1;
    tdma_task.period.nano_secs = 0;
    tdma_task.cpu_reserve.secs = 0;      // tdma reserve , 0 to disable
    tdma_task.cpu_reserve.nano_secs = 0;
    tdma_task.offset.secs = 0;
    tdma_task.offset.nano_secs = 0;
//    #ifdef DEBUG
    //printf( "tdma activate. I am %d\r\n" ,my_addr16);
//    #endif 
    nrk_activate_task (&tdma_task);
}
