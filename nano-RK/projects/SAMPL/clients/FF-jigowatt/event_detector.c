#include <event_detector.h>
#include <nrk.h>
#include <transducer_handler.h>
#include <transducer_registry.h>
#include <ff_power.h>
#include <power_vars.h>

static uint8_t async_buf[40];
static FF_POWER_DEBUG_PKT pd_pkt;

static TRANSDUCER_MSG_T tran_msg;
static TRANSDUCER_PKT_T tran_pkt;

uint8_t current_last, current_last2;

void event_detector_task()
{
uint8_t len,val,delta,delta2;

while (!sampl_started())
    nrk_wait_until_next_period ();

  tran_pkt.msgs_payload=async_buf;
  tran_pkt.num_msgs=0;

   current_last=rms_current;
   current_last2=rms_current2;

while(1)
{
if(rms_current>current_last) delta=rms_current-current_last;
else delta=current_last-rms_current;

if(rms_current2>current_last2) delta2=rms_current2-current_last2;
else delta2=current_last2-rms_current2;

if(delta>2 || delta2>2)
{
	pd_pkt.type=DEBUG_PKT;
        pd_pkt.rms_voltage=rms_voltage;
        pd_pkt.rms_current=rms_current;
        pd_pkt.rms_current2=rms_current2;
        pd_pkt.true_power=true_power;
        pd_pkt.true_power2=true_power2;
        pd_pkt.freq=freq;
        pd_pkt.energy=tmp_energy;
        pd_pkt.energy2=tmp_energy2;
        pd_pkt.current_p2p_high=l_c_p2p_high;
        pd_pkt.current_p2p_low=l_c_p2p_low;
        pd_pkt.current_p2p_high2=l_c_p2p_high2;
        pd_pkt.current_p2p_low2=l_c_p2p_low2;
        pd_pkt.voltage_p2p_high=l_v_p2p_high;
        pd_pkt.voltage_p2p_low=l_v_p2p_low;
        pd_pkt.total_secs=total_secs;


tran_pkt.num_msgs=0;
tran_pkt.checksum=0;
tran_pkt.msgs_payload=&(async_buf[TRANSDUCER_PKT_HEADER_SIZE]);

tran_msg.payload=&(async_buf[TRANSDUCER_PKT_HEADER_SIZE+TRANSDUCER_MSG_HEADER_SIZE]);
tran_msg.type=TRAN_POWER_PKT;
tran_msg.len=ff_power_debug_pack(tran_msg.payload, &pd_pkt);
tran_msg.mac_addr=my_mac;
//printf( "sending: %d\r\n",my_mac);
len=transducer_msg_add( &tran_pkt, &tran_msg);

len = transducer_pkt_pack(&tran_pkt, async_buf);
val=push_p2p_pkt( async_buf,len,TRANSDUCER_PKT, 0x0 );
}

current_last=rms_current;
current_last2=rms_current2;
nrk_wait_until_next_period();
}

}
