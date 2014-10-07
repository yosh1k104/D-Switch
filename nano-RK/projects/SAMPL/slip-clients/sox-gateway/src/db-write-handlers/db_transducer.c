#include <stdlib.h>
#include <time.h>
#include <sampl.h>
#include <globals.h>
#include <ff_basic_sensor_pkt.h>
#include <ff_basic_sensor_cal.h>
#include <jiga_watt_cal.h>
#include <transducer_pkt.h>
#include <transducer_registry.h>
#include <ff_power.h>
#include <tx_queue.h>
#include <error_log.h>
#include <ffdb.h>

#if SOX_SUPPORT
#include <xmpp_transducer.h>
#endif

#define TEMPERATURE_OFFSET 400


static uint8_t freq;
static uint16_t adc_rms_current, adc_rms_voltage;
static uint16_t adc_rms_current2;
static uint32_t adc_true_power, adc_energy, time_secs;
static uint32_t adc_true_power2, adc_energy2;
static uint16_t adc_v_p2p_low, adc_v_p2p_high;
static uint16_t adc_i_p2p_low, adc_i_p2p_high;
static uint16_t adc_i_p2p_low2, adc_i_p2p_high2;

static float cal_energy, rms_voltage, rms_current, true_power, apparent_power, power_factor;
static float rms_current2, true_power2, apparent_power2, power_factor2;

static jiga_watt_cal_struct_t jiga_watt_cal;



void db_write_transducer_pkt (SAMPL_GATEWAY_PKT_T * gw_pkt)
{
uint8_t i;
int8_t v;
static struct firefly_env ff_env;
static struct firefly_pow ff_pow;
char mac_id_name[32];
FF_SENSOR_SHORT_PKT_T sensor_short;
TRANSDUCER_PKT_T tp;
TRANSDUCER_MSG_T tm;
FF_POWER_SENSE_PKT ps;
FF_POWER_DEBUG_PKT pd;
float temp_cal, battery_cal;
uint16_t light_cal;
uint32_t c_mac;

if(gw_pkt->pkt_type!=TRANSDUCER_PKT) return 0;
        // GW reply packets do not include the tran_pkt header
        // since this information can be captured in the gw
        // packet header info.  So fill out a dummy packet
    v=transducer_pkt_unpack(&tp, gw_pkt->payload );
    if(v!=1) {
        printf( "Transducer checksum failed!\n" );
        }
    for (i = 0; i < tp.num_msgs; i++) {
      v=transducer_msg_get (&tp,&tm, i);
      printf( "tm_mac=%u\n",tm.mac_addr);
      switch (tm.type) {
      case TRAN_FF_BASIC_SHORT:
        ff_basic_sensor_short_unpack (&tm, &sensor_short);
 
        c_mac =
          gw_pkt->subnet_mac[2] << 24 | gw_pkt->
          subnet_mac[1] << 16 | gw_pkt->subnet_mac[0] << 8 | tm.mac_addr;

	// calibrate temperature
        temp_cal =
          ff_basic_sensor_cal_get_temp ((sensor_short.temperature << 1) +
                                        TEMPERATURE_OFFSET) +
          ff_basic_sensor_cal_get_temp_offset (c_mac);
        
	// calibrate light
        printf ("battery=%u light=%u\n", sensor_short.battery,
                sensor_short.light);
        light_cal =
          ff_basic_sensor_cal_get_light (sensor_short.light,
                                         ((float)
                                          (sensor_short.battery +
                                           100)) / 100) +
          ff_basic_sensor_cal_get_light_offset (c_mac);
        light_cal = 255 - light_cal;

	// adjust battery	
        battery_cal = ((float) (sensor_short.battery + 100)) / 100;
        


        printf ("Sensor pkt from 0x%x\n", tm.mac_addr);
        printf ("   Light: %u (%u)\n", light_cal, (uint8_t)sensor_short.light);
        printf ("   Temperature: %f (%u)\n", temp_cal, (uint8_t)sensor_short.temperature);
        printf ("   Acceleration: %u\n", (uint8_t)sensor_short.acceleration);
        printf ("   Sound Level: %u\n", (uint8_t)sensor_short.sound_level);
        printf ("   Battery: %f (%u)\n",battery_cal, (uint8_t)sensor_short.battery + 100);
       	sprintf(mac_id_name,"ff_%02x%02x%02x%02x",gw_pkt->subnet_mac[2],gw_pkt->subnet_mac[1],gw_pkt->subnet_mac[0],tm.mac_addr);
	ff_env.id=mac_id_name;
	ff_env.time=time(NULL);
	ff_env.light=(int)light_cal;
	ff_env.temp=(int)temp_cal;
	ff_env.accl=sensor_short.acceleration;
	ff_env.voltage=(int)(battery_cal*100);
	ff_env.audio=sensor_short.sound_level;
	write_env(ff_env);
        break;
      case TRAN_ACK:
        printf( "Transducer ACK from 0x%x\n",tm.mac_addr );
        break;
      case TRAN_NCK:
        printf( "Transducer NCK from 0x%x\n",tm.mac_addr );
        break;
      case TRAN_POWER_PKT:
           if(tm.payload[0]==SENSE_PKT)
              {
                        ff_power_sense_unpack( tm.payload, &ps);
	
        		c_mac =
          			gw_pkt->subnet_mac[2] << 24 | gw_pkt->
          			subnet_mac[1] << 16 | gw_pkt->subnet_mac[0] << 8 | tm.mac_addr;
        		if (jiga_watt_cal_get (c_mac,ps.socket_num, &jiga_watt_cal) == 1) {
          			rms_voltage = (float)ps.rms_voltage / jiga_watt_cal.voltage_scaler;
          			rms_current = (float)ps.rms_current / jiga_watt_cal.current_scaler;
          			true_power = (float)ps.true_power / jiga_watt_cal.power_scaler;
				cal_energy=(float)ps.energy / jiga_watt_cal.power_scaler;  // get Watt Seconds
				cal_energy=cal_energy/3600; // convert to WHrs
        		}
        		else {
				cal_energy=0;
          			rms_voltage = 0;
          			rms_current = 0;
          			jiga_watt_cal.current_adc_offset = 0;
        		}

        		if (ps.rms_current <= jiga_watt_cal.current_adc_offset) {
          		// disconnected state
          		//adc_rms_current = 0;
          			rms_current = 0;
          			true_power = 0;
          			apparent_power = 0;
        		}

        		apparent_power = rms_voltage * rms_current;
        		//if (true_power > apparent_power)
        		//  apparent_power = true_power;
        		power_factor = true_power / apparent_power;

                        printf( "Socket %d:\n",ps.socket_num );
                        printf( "\tsocket0 state: %u\n",ps.socket0_state);
                        printf( "\tsocket1 state: %u\n",ps.socket1_state);
                        printf( "\trms_current: %f (%u)\n",rms_current,ps.rms_current);
                        printf( "\trms_voltage: %f (%u)\n",rms_voltage,ps.rms_voltage);
                        printf( "\ttrue_power: %f (%u)\n",true_power,ps.true_power);
        		printf ("\tappearent power: %f VA\n", apparent_power);
        		printf ("\tpower factor:  %f VA\n", power_factor);
                        printf( "\tenergy: %f (%u)\n",cal_energy,ps.energy);
       			sprintf(mac_id_name,"ff_%02x%02x%02x%02x_%d",gw_pkt->subnet_mac[2],gw_pkt->subnet_mac[1],gw_pkt->subnet_mac[0],tm.mac_addr,ps.socket_num );
			ff_pow.id=mac_id_name;
			ff_pow.time=time(NULL);
			if(ps.socket_num==0)
				ff_pow.state=ps.socket0_state;
			else
				ff_pow.state=ps.socket1_state;
			ff_pow.rms_current=(int)rms_current;
			ff_pow.rms_voltage=(int)rms_voltage;
			ff_pow.true_power=(int)true_power;
			ff_pow.energy=(int)cal_energy;
			write_pow(ff_pow);
       } else
              if(tm.payload[0]==DEBUG_PKT)
                 {
                        ff_power_debug_unpack( tm.payload, &pd);

        		c_mac =
          			gw_pkt->subnet_mac[2] << 24 | gw_pkt->
          			subnet_mac[1] << 16 | gw_pkt->subnet_mac[0] << 8 | tm.mac_addr;

        		if (jiga_watt_cal_get (c_mac,0, &jiga_watt_cal) == 1) {
          			rms_voltage = (float)pd.rms_voltage / jiga_watt_cal.voltage_scaler;
          			rms_current = (float)pd.rms_current / jiga_watt_cal.current_scaler;
          			true_power = (float)pd.true_power / jiga_watt_cal.power_scaler;
				cal_energy=(float)pd.energy / jiga_watt_cal.power_scaler;  // get Watt Seconds
				cal_energy=cal_energy/3600; // convert to WHrs
        		}
        		else {
				cal_energy=0;
          			rms_voltage = 0;
          			rms_current = 0;
          			jiga_watt_cal.current_adc_offset = 0;
        		}

        		if (pd.rms_current <= jiga_watt_cal.current_adc_offset) {
          		// disconnected state
          		//adc_rms_current = 0;
          			rms_current = 0;
          			true_power = 0;
          			apparent_power = 0;
        		}

        		apparent_power = rms_voltage * rms_current;
        		//if (true_power > apparent_power)
        		//  apparent_power = true_power;
        		power_factor = true_power / apparent_power;



       			sprintf(mac_id_name,"ff_%02x%02x%02x%02x_0",gw_pkt->subnet_mac[2],gw_pkt->subnet_mac[1],gw_pkt->subnet_mac[0],tm.mac_addr);
			ff_pow.id=mac_id_name;
			ff_pow.time=time(NULL);
			ff_pow.state=-1;
			ff_pow.rms_current=(int)rms_current;
			ff_pow.rms_voltage=(int)rms_voltage;
			ff_pow.true_power=(int)true_power;
			ff_pow.energy=(int)cal_energy; //pd.energy;
			write_pow(ff_pow);

                        printf( "\trms_voltage (all): %f (%u)\n",rms_voltage, pd.rms_voltage);
                        printf( "\tfreq (all): %u\n",pd.freq);
                        printf( "\trms_current: %f (%u)\n",rms_current, pd.rms_current);
                        printf( "\ttrue_power: %f (%u)\n",true_power,pd.true_power);
                        printf( "\tenergy: %f (%u)\n",cal_energy,pd.energy);





        		if (jiga_watt_cal_get (c_mac,1, &jiga_watt_cal) == 1) {
          			rms_voltage = (float)pd.rms_voltage / jiga_watt_cal.voltage_scaler;
          			rms_current = (float)pd.rms_current2 / jiga_watt_cal.current_scaler;
          			true_power = (float)pd.true_power2 / jiga_watt_cal.power_scaler;
				cal_energy=(float)pd.energy2 / jiga_watt_cal.power_scaler;  // get Watt Seconds
				cal_energy=cal_energy/3600; // convert to WHrs
        		}
        		else {
				cal_energy=0;
          			rms_voltage = 0;
          			rms_current = 0;
          			jiga_watt_cal.current_adc_offset = 0;
        		}

        		if (pd.rms_current2 <= jiga_watt_cal.current_adc_offset) {
          		// disconnected state
          		//adc_rms_current = 0;
          			rms_current = 0;
          			true_power = 0;
          			apparent_power = 0;
        		}

        		apparent_power = rms_voltage * rms_current;
        		//if (true_power > apparent_power)
        		//  apparent_power = true_power;
        		power_factor = true_power / apparent_power;





       			sprintf(mac_id_name,"ff_%02x%02x%02x%02x_1",gw_pkt->subnet_mac[2],gw_pkt->subnet_mac[1],gw_pkt->subnet_mac[0],tm.mac_addr);
			ff_pow.id=mac_id_name;
			ff_pow.time=time(NULL);
			ff_pow.state=-1;
			ff_pow.rms_current=(int)rms_current;
			ff_pow.rms_voltage=(int)rms_voltage;
			ff_pow.true_power=(int)true_power;
			ff_pow.energy=(int)cal_energy;
			//ff_pow.energy=pd.energy2;
			write_pow(ff_pow);

                        printf( "\trms_current2: %f (%u)\n",rms_current,pd.rms_current2);
                        printf( "\ttrue_power2: %f (%u)\n",true_power,pd.true_power2);
                        printf( "\tenergy2: %f (%u)\n",cal_energy,pd.energy2);
                        printf( "\ttime: %u\n",pd.total_secs);
               }
                 else
                 printf( "Unkown Power msg type %u\n", tm.payload[0]);

                break;


      default:
        printf ("unkown transducer packet: %u\n",tm.type);
      }
    }


}
