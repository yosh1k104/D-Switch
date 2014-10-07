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
*******************************************************************************/


#ifndef _TDMA_H
#define _TDMA_H
#include <include.h>
#include <basic_rf.h>
#include <nrk.h>

/************************************************************************

TDMA-ASAP is a TDMA protocol featuring slot stealing and parallelism.
Scheduling can be done manually or determined via automatic tree creation.
Slot stealing can be performed among siblings in a tree when the owner of a
slot has nothing to transmit. TDMA-ASAP currently assumes data is propogated
from leaf to root, while time synchronization packets are sent from root
to children periodically. 

This implementation currently provides one RX and one TX buffer, and works with
16-bit addresses.

************************************************************************/


// this should be specified in the project's file
//#define TDMA_MODE TDMA_SLAVE
//#define TDMA_MODE TDMA_MASTER

// these will eventually be removed
#define TDMA_MASTER_MODE
#define TDMA_SLAVE_MODE


// to shrink code size a bit
/*
#if   defined(TDMA_MODE) && (TDMA_MODE == TDMA_MASTER)
    #define TDMA_MASTER_MODE // compile master functions
#endif

#if defined(TDMA_MODE) && (TDMA_MODE == TDMA_SLAVE)
    #define TDMA_SLAVE_MODE // compile slave-only functions
#endif
*/


// for debugging and making
// multi-level trees without having to
// place nodes far away
// (makes nodes require to be really close
// to consider them children)
//#define DEBUG_RSSI

#define TDMA_CCA_THRESH        (-49) 
                       
#define TDMA_MAX_PKT_SIZE		116
#define TDMA_STACK_SIZE 	    	512
#define TDMA_DEFAULT_CHECK_RATE_MS 	100 
#define TDMA_TASK_PRIORITY		20 

// max number of children/neighbors to keep track of
#define TREE_MAX_CHILDREN 10
#define TREE_MAX_NEIGHBORS 15 

// Whether to do Slot stealing
// right now, omitting slot stealing doesn't result in many
// optimizations during RX slots. Potential stealers will
// NOT wake up for stealable slots, but the microslot times will
// still be observed.
#define SLOT_STEALING

//#define TDMA_RX_WAIT_MS       		7 //shooting for 2
//#define TX_GUARD_TIME_US            4000 // shooting for 500?

#define TDMA_RX_WAIT_MS       		8 //shooting for 2
#define TX_GUARD_TIME_US            3000 // shooting for 500?

#define TDMA_DELTA_SLOT_MS 3 
#define TDMA_SLOT_DATA_TIME_MS 10

#define TDMA_SLOT_TIME_MS		((TREE_MAX_CHILDREN * TDMA_DELTA_SLOT_MS)\
                                 + TDMA_SLOT_DATA_TIME_MS)
#define TDMA_SLOTS_PER_CYCLE	    256
//#define TDMA_SLOTS_PER_CYCLE		1024
#define TDMA_SYNC_THRESHOLD_SECS	120

// addr is 2 bytes
#define ADDR_SIZE 2

// Fill this in with all packet fields.
typedef enum {
	// Global slot requires 10 bits to hold 1024 slots
	// hence it uses 2 bytes.
	TDMA_SLOT=0,
	// This token is used to ensure that a node never
	// synchronizes off of a node that has an older sync 
	// value.
	TDMA_TIME_TOKEN=2,
	TDMA_DATA_START=3
} tdma_pkt_field_t;

typedef struct {
	int8_t length;
	uint8_t *pPayload;
} TDMA_TX_INFO;

// node type
typedef enum {
	TDMA_MASTER,
	TDMA_SLAVE
} tdma_node_mode_t;

typedef struct {
    uint16_t mac_addr;
    uint8_t channel;
    uint8_t power;
    uint16_t tx_guard_time;
    uint16_t rx_timeout;
    uint8_t mobile_sync_timeout;
} tdma_param_t;

tdma_param_t tdma_param;

typedef enum {
    TDMA_SCHED_TREE,
    TDMA_SCHED_MANUAL
} tdma_sched_method_t;

tdma_node_mode_t tdma_my_mode_get();

// Structures
nrk_task_type tdma_task;
NRK_STK tdma_task_stack[TDMA_STACK_SIZE];

RF_RX_INFO tdma_rfRxInfo;
RF_TX_INFO tdma_rfTxInfo; 

// Signals
nrk_sig_t tdma_rx_pkt_signal;
nrk_sig_t tdma_tx_pkt_done_signal;
nrk_sig_t tdma_enable_signal;

nrk_sig_t tdma_get_tx_done_signal();
nrk_sig_t tdma_get_rx_pkt_signal();

// Initialization
void tdma_task_config ();
int8_t tdma_started();
int8_t tdma_init(uint8_t chan);

// Config
int8_t tdma_rx_pkt_set_buffer(uint8_t *buf, uint8_t size);
int8_t tdma_schedule_method_set(tdma_sched_method_t m);
int8_t tdma_mode_set(tdma_node_mode_t mode);
int8_t tdma_set_channel(uint8_t chan);

// tree-related
uint8_t tdma_tree_level_get();
uint16_t tdma_mac_get();

// RX and TX
int8_t tdma_tx_pkt_check();
int8_t tdma_rx_pkt_check();

uint8_t *tdma_rx_pkt_get(uint8_t *len, int8_t *rssi, uint8_t *slot);
int8_t tdma_tx_pkt(uint8_t *buf, uint8_t len);
int8_t tdma_rx_pkt_release(void);
int8_t tdma_set_rf_power(uint8_t power);

// Waiting
int8_t tdma_wait_until_rx_or_tx();
int8_t tdma_wait_until_tx();
int8_t tdma_wait_until_rx_pkt();

#endif
