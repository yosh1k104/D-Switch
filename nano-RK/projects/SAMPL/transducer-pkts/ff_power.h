#ifndef FF_POWER_H
#define FF_POWER_H

#include <globals.h>
#include <sampl.h>
#include "transducer_registry.h"
#include <transducer_pkt.h>

#define SENSE_PKT	1
#define ACTUATE_PKT	2
#define DEBUG_PKT	3
#define SENSE_RQST_PKT	4

#define SOCKET_HOLD	0
#define SOCKET_ON	1
#define SOCKET_OFF	2

typedef struct ff_power_sense_pkt
{
	uint8_t type; // Byte 0 (type == SENSE_PKT)
	uint8_t socket_num; 
	uint8_t socket0_state; 
	uint8_t socket1_state; 
	uint16_t rms_current;  // Byte 2,3
	uint16_t rms_voltage;  // Byte 4,5
	uint32_t true_power;   // Byte 6,7,8
	uint32_t energy;       // Byte 9,10,11,12
} FF_POWER_SENSE_PKT;

typedef struct ff_power_actuate_pkt
{
	uint8_t type; // Byte 0 (type == ACTUATE_PKT)
	uint8_t socket0_state;  // Byte 1
	uint8_t socket1_state;  // Byte 2
} FF_POWER_ACTUATE_PKT;

typedef struct ff_power_request_pkt
{
	uint8_t type; 	   // Byte 0 
	uint8_t socket;    // Byte 1
	uint8_t pkt_type;  // Byte 2
} FF_POWER_RQST_PKT;



typedef struct ff_power_debug_pkt
{
	uint8_t type;   // Byte 0 (type=DEBUG_PKT)
	uint16_t rms_current;
	uint16_t rms_voltage;
	uint32_t true_power;
	uint32_t energy;
	uint16_t current_p2p_high;	
	uint16_t current_p2p_low;	
	uint16_t rms_current2;	
	uint32_t true_power2;	
	uint32_t energy2;	
	uint16_t current_p2p_high2;
	uint16_t current_p2p_low2;
	uint16_t voltage_p2p_low;
	uint16_t voltage_p2p_high;
	uint8_t  freq;
	uint32_t total_secs;
} FF_POWER_DEBUG_PKT;




uint8_t ff_power_debug_pack( uint8_t *payload, FF_POWER_DEBUG_PKT *p );
uint8_t ff_power_debug_unpack( uint8_t *payload, FF_POWER_DEBUG_PKT *p );
uint8_t ff_power_sense_pack( uint8_t *payload, FF_POWER_SENSE_PKT *p );
uint8_t ff_power_sense_unpack( uint8_t *payload, FF_POWER_SENSE_PKT *p );
uint8_t ff_power_actuate_pack( uint8_t *payload, FF_POWER_ACTUATE_PKT *p );
uint8_t ff_power_actuate_unpack( uint8_t *payload, FF_POWER_ACTUATE_PKT *p );
uint8_t ff_power_rqst_pack( uint8_t *payload, FF_POWER_RQST_PKT *p );
uint8_t ff_power_rqst_unpack( uint8_t *payload, FF_POWER_RQST_PKT *p );

#endif
