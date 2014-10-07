#include "ff_power.h"
#include "transducer_registry.h"
#include <transducer_pkt.h>
#include <stdio.h>

uint8_t ff_power_actuate_pack(uint8_t *payload,
                                    FF_POWER_ACTUATE_PKT * p)
{
	payload[0]=ACTUATE_PKT;
	payload[1]=p->socket0_state;
	payload[2]=p->socket1_state;
  return 3;
}

uint8_t ff_power_actuate_unpack(uint8_t *payload,
                                    FF_POWER_ACTUATE_PKT * p)
{
	if(payload[0]!=ACTUATE_PKT) return 0;
	if(p==NULL || payload==NULL) return 0;
	p->type=payload[0];
	p->socket0_state=payload[1];
	p->socket1_state=payload[2];
  return 3;
}


uint8_t ff_power_rqst_pack(uint8_t *payload,
                                    FF_POWER_RQST_PKT * p)
{
	payload[0]=SENSE_RQST_PKT;
	payload[1]=p->socket;
	payload[2]=p->pkt_type;
  return 3;
}

uint8_t ff_power_rqst_unpack(uint8_t *payload,
                                    FF_POWER_RQST_PKT * p)
{
	if(payload[0]!=SENSE_RQST_PKT) return 0;
	if(p==NULL || payload==NULL) return 0;
	p->type=payload[0];
	p->socket=payload[1];
	p->pkt_type=payload[2];
  return 3;
}





uint8_t ff_power_sense_pack(uint8_t *payload,
                                    FF_POWER_SENSE_PKT * p)
{
	payload[0]=SENSE_PKT;
	payload[1]=(p->socket_num<<4) | (p->socket1_state<<1) | p->socket0_state;
	payload[2]=(p->rms_current>>8)&0xff;
        payload[3]=p->rms_current&0xff;
	payload[4]=(p->rms_voltage>>8)&0xff;
        payload[5]=p->rms_voltage&0xff;
        payload[6]=(p->true_power>>16)&0xff;
        payload[7]=(p->true_power>>8)&0xff;
        payload[8]=(p->true_power)&0xff;
        payload[9]=(p->energy>>24)&0xff;
        payload[10]=(p->energy>>16)&0xff;
        payload[11]=(p->energy>>8)&0xff;
        payload[12]=(p->energy)&0xff;
  return 13;
}

uint8_t ff_power_sense_unpack(uint8_t *payload,
                                    FF_POWER_SENSE_PKT * p)
{
	if(payload[0]!=SENSE_PKT) return 0;
	if(p==NULL || payload==NULL) return 0;
	p->type=payload[0];
	p->socket_num=payload[1]>>4;
	p->socket0_state=payload[1]&0x1;
	p->socket1_state=(payload[1]&0x2)>>1;
	p->rms_current=(payload[2]<<8) | payload[3];
	p->rms_voltage=(payload[4]<<8) | payload[5];
	p->true_power=(payload[6]<<16) | (payload[7]<<8) | payload[8];
	p->energy=(payload[9]<<24) | (payload[10]<<16) | (payload[11]<<8) | payload[12];
  return 13;
}

uint8_t ff_power_debug_pack(uint8_t *payload,
                                    FF_POWER_DEBUG_PKT * p)
{
	payload[0]=DEBUG_PKT;
	payload[1]=(p->rms_current>>8)&0xff;
        payload[2]=p->rms_current&0xff;
        payload[3]=(p->true_power>>16)&0xff;
        payload[4]=(p->true_power>>8)&0xff;
        payload[5]=(p->true_power)&0xff;
        payload[6]=(p->energy>>24)&0xff;
        payload[7]=(p->energy>>16)&0xff;
        payload[8]=(p->energy>>8)&0xff;
        payload[9]=(p->energy)&0xff;
        payload[10]=(p->current_p2p_high>>8)&0xff;
        payload[11]=(p->current_p2p_high)&0xff;
        payload[12]=(p->current_p2p_low>>8)&0xff;
        payload[13]=(p->current_p2p_low)&0xff;
        payload[14]=(p->rms_current2>>8)&0xff;
        payload[15]=p->rms_current2&0xff;
        payload[16]=(p->true_power2>>16)&0xff;
        payload[17]=(p->true_power2>>8)&0xff;
        payload[18]=(p->true_power2)&0xff;
        payload[19]=(p->energy2>>24)&0xff;
        payload[20]=(p->energy2>>16)&0xff;
        payload[21]=(p->energy2>>8)&0xff;
        payload[22]=(p->energy2)&0xff;
        payload[23]=(p->current_p2p_high2>>8)&0xff;
        payload[24]=(p->current_p2p_high2)&0xff;
        payload[25]=(p->current_p2p_low2>>8)&0xff;
        payload[26]=(p->current_p2p_low2)&0xff;
        payload[27]=(p->voltage_p2p_high>>8)&0xff;
        payload[28]=(p->voltage_p2p_high)&0xff;
        payload[29]=(p->voltage_p2p_low>>8)&0xff;
        payload[30]=(p->voltage_p2p_low)&0xff;
        payload[31]=(p->rms_voltage>>8)&0xff;
        payload[32]=p->rms_voltage&0xff;
        payload[33]=p->freq&0xff;
        payload[34]=(p->total_secs>>24)&0xff;
        payload[35]=(p->total_secs>>16)&0xff;
        payload[36]=(p->total_secs>>8)&0xff;
        payload[37]=(p->total_secs)&0xff;
  return 38;
}

uint8_t ff_power_debug_unpack(uint8_t *payload,
                                    FF_POWER_DEBUG_PKT * p)
{
	if(payload[0]!=DEBUG_PKT) return 0;
	if(p==NULL || payload==NULL ) return 0;
	p->type=payload[0];
	p->rms_current=(payload[1]<<8) | payload[2];
	p->true_power=(payload[3]<<16) | (payload[4]<<8) | payload[5];
	p->energy=(payload[6]<<24) | (payload[7]<<16) | (payload[8]<<8) | payload[9];
	p->current_p2p_high=(payload[10]<<8) | payload[11];
	p->current_p2p_low=(payload[12]<<8) | payload[13];
	p->rms_current2=(payload[14]<<8) | payload[15];
	p->true_power2=(payload[16]<<16) | (payload[17]<<8) | payload[18];
	p->energy2=(payload[19]<<24) | (payload[20]<<16) | (payload[21]<<8) | payload[22];
	p->current_p2p_high2=(payload[23]<<8) | payload[24];
	p->current_p2p_low2=(payload[25]<<8) | payload[26];
	p->voltage_p2p_high=(payload[27]<<8) | payload[28];
	p->voltage_p2p_low=(payload[29]<<8) | payload[30];
	p->rms_voltage=(payload[31]<<8) | payload[32];
	p->freq = payload[33];
	p->total_secs=(payload[34]<<24) | (payload[35]<<16) | (payload[36]<<8) | payload[37];
  return 38;
}
