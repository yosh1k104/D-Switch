#include <stdlib.h>
#include <time.h>
#include <xmpp_transducer.h>
#include <sampl.h>
#include <soxlib.h>
#include <node_cache.h>
#include <globals.h>
#include <xmpp_pkt.h>
#include <ack_pkt.h>
#include <ff_basic_sensor_pkt.h>
#include <ff_basic_sensor_cal.h>
#include <jiga_watt_cal.h>
#include <transducer_pkt.h>
#include <transducer_registry.h>
#include <ff_power.h>
#include <xmpp_proxy.h>
#include <tx_queue.h>
#include <error_log.h>


#define TEMPERATURE_OFFSET 400


static uint8_t freq;
static uint16_t adc_rms_current, adc_rms_voltage;
static uint16_t adc_rms_current2;
static uint32_t adc_true_power, adc_energy, time_secs;
static uint32_t adc_true_power2, adc_energy2;
static uint16_t adc_v_p2p_low, adc_v_p2p_high;
static uint16_t adc_i_p2p_low, adc_i_p2p_high;
static uint16_t adc_i_p2p_low2, adc_i_p2p_high2;

static float rms_voltage, rms_current, true_power, apparent_power, power_factor;
static float rms_current2, true_power2, apparent_power2, power_factor2;

static jiga_watt_cal_struct_t jiga_watt_cal;

void publish_xmpp_transducer_pkt (SAMPL_GATEWAY_PKT_T * gw_pkt)
{
uint8_t i;
int8_t v;
FF_SENSOR_SHORT_PKT_T sensor_short;
ACK_PKT_T ack;
TRANSDUCER_PKT_T tp;
TRANSDUCER_MSG_T tm;
FF_POWER_SENSE_PKT ps;
FF_POWER_DEBUG_PKT pd;

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
      switch (tm.type) {
      case TRAN_FF_BASIC_SHORT:
        ff_basic_sensor_short_unpack (&tm, &sensor_short);
        printf ("Sensor pkt from 0x%x\n", tm.mac_addr);
        printf ("   Light: %d\n", sensor_short.light);
        printf ("   Temperature: %d\n", sensor_short.temperature);
        printf ("   Acceleration: %d\n", sensor_short.acceleration);
        printf ("   Sound Level: %d\n", sensor_short.sound_level);
        printf ("   Battery: %d\n", sensor_short.battery + 100);
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
                        printf( "Socket %d:\n",ps.socket_num );
                        printf( "\tsocket0 state: %d\n",ps.socket0_state);
                        printf( "\tsocket1 state: %d\n",ps.socket1_state);
                        printf( "\trms_current: %d\n",ps.rms_current);
                        printf( "\trms_voltage: %d\n",ps.rms_voltage);
                        printf( "\ttrue_power: %d\n",ps.true_power);
                        printf( "\tenergy: %d\n",ps.energy);
              } else
              if(tm.payload[0]==DEBUG_PKT)
                 {
                        ff_power_debug_unpack( tm.payload, &pd);
                        printf( "\trms_voltage (all): %d\n",pd.rms_voltage);
                        printf( "\tfreq (all): %d\n",pd.freq);
                        printf( "\trms_current: %d\n",pd.rms_current);
                        printf( "\ttrue_power: %d\n",pd.true_power);
                        printf( "\tenergy: %d\n",pd.energy);
                        printf( "\trms_current2: %d\n",pd.rms_current2);
                        printf( "\ttrue_power2: %d\n",pd.true_power2);
                        printf( "\tenergy2: %d\n",pd.energy2);
                        printf( "\ttime: %d\n",pd.total_secs);
                 }
                 else
                 printf( "Unkown Power msg type %d\n", tm.payload[0]);

                break;


      default:
        printf ("unkown transducer packet: %d\n",tm.type);
      }
    }


}
/*  FF_SENSOR_SHORT_PKT_T sensor_short;
  ACK_PKT_T ack;
  TRANSDUCER_REPLY_PKT_T tran_pkt;
  int i, val;
  uint8_t t;
  SOXMessage *msg = NULL;
  char sensor_raw_str[32];
  char sensor_adj_str[32];
  char sensor_type_str[32];
  char event_node[32];
  char reg_name[32];
  char time_str[100];
  char reg_id[100];
  int ret = 0;
  float temp_cal, battery_cal;
  uint16_t light_cal;
  uint32_t c_mac;
  time_t timestamp;

  //      unpack_gateway_packet(&gw_pkt );
  // You will have a gateway packet here to operate on.
  // The gateway packet has a payload which contains user defined packets.

  // Lets print the raw packet:
  //print_gw_packet(&gw_pkt);

  // Now lets parse out some application data:
  switch (gw_pkt->pkt_type) {
    // PING and ACK are the same
  case PING_PKT:
  case ACK_PKT:
    for (i = 0; i < gw_pkt->num_msgs; i++) {
      t = ack_pkt_get (&ack, gw_pkt->payload, i);
      printf ("Ack pkt from 0x%x\n", ack.mac_addr);
    }
    break;

    // Default FireFly sensor packet
  case TRANSDUCER_PKT:
    printf ("gw_pkt->num_msgs=%d\n", gw_pkt->num_msgs);
    for (i = 0; i < gw_pkt->num_msgs; i++) {
      transducer_reply_pkt_get (&tran_pkt, gw_pkt->payload, i);

      switch (tran_pkt.type) {
      case TRAN_ACK:
        printf ("ACK 0x%x\n", tran_pkt.mac_addr);
        break;
      case TRAN_NCK:
        printf ("NCK 0x%x\n", tran_pkt.mac_addr);
        break;

      case TRAN_BINARY_SENSOR:
	{
	uint8_t binary_sensor_type, binary_sensor_value;
 
	time (&timestamp);
        strftime (time_str, 100, "%Y-%m-%dT%X", localtime (&timestamp));
        printf ("Binary Sensor from 0x%x\n", tran_pkt.mac_addr);
        binary_sensor_type=tran_pkt.payload[0]&0x7f;
        binary_sensor_value=tran_pkt.payload[0]>>7;
	printf( "     binary sensor type: %d value: %d\n", binary_sensor_type, binary_sensor_value ); 

        sprintf (event_node, "%02x%02x%02x%02x", gw_pkt->subnet_mac[2],
                 gw_pkt->subnet_mac[1],
                 gw_pkt->subnet_mac[0], tran_pkt.mac_addr);


        check_and_create_node (event_node);
        val = reg_id_get (event_node, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg = create_sox_message ();
        msg_add_device_installation (msg, event_node, reg_id, "FIREFLY",
                                     "A Firefly Node", time_str);


	if(binary_sensor_type==BINARY_SENSOR_MOTION) sprintf (sensor_type_str, "Motion");
	if(binary_sensor_type==BINARY_SENSOR_GPIO) sprintf (sensor_type_str, "GPIO");
	if(binary_sensor_type==BINARY_SENSOR_LEAK) sprintf (sensor_type_str, "Water");
	if(binary_sensor_value==1) strcpy(sensor_raw_str,"true"); else strcpy(sensor_raw_str,"false");
        sprintf (sensor_adj_str, "%d", binary_sensor_value);

        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0001", reg_id, FALSE);
        msg_add_value_to_transducer (msg, event_node, "0001", sensor_adj_str, sensor_raw_str, time_str);

	if(xmpp_flag)
	{
        ret = publish_sox_message (connection, event_node, msg);
        delete_sox_message (msg);
        if (ret != XMPP_NO_ERROR) {
          sprintf (global_error_msg, "Could not publish Jiga Watt to %s: %s",
                   event_node, ERROR_MESSAGE (ret));
          log_write (global_error_msg);
          return -1;
        }
	}
	}

	break;

      case TRAN_POWER_SENSE:
	{
	uint8_t outlet, outlet_state;
        float rms_voltage, rms_current, true_power, apparent_power, power_factor,energy;
        
	time (&timestamp);
        strftime (time_str, 100, "%Y-%m-%dT%X", localtime (&timestamp));
        printf ("Power Sense from 0x%x\n", tran_pkt.mac_addr);
        outlet=tran_pkt.payload[0]&0x0f;
        outlet_state=tran_pkt.payload[0]>>4;
	adc_rms_current = (tran_pkt.payload[1] << 8) | tran_pkt.payload[2];
        adc_rms_voltage = (tran_pkt.payload[3] << 8) | tran_pkt.payload[4];
        adc_true_power =
          (tran_pkt.
           payload[5] << 16) | (tran_pkt.payload[6] << 8) |
          (tran_pkt.payload[7]);
         adc_energy =
          (tran_pkt.
           payload[8] << 24) | (tran_pkt.payload[9] << 16) |
          (tran_pkt.payload[10] << 8) | (tran_pkt.payload[11]);


        c_mac =
          gw_pkt->subnet_mac[2] << 24 | gw_pkt->
          subnet_mac[1] << 16 | gw_pkt->subnet_mac[0] << 8 | tran_pkt.
          mac_addr;

        if (jiga_watt_cal_get (c_mac,0, &jiga_watt_cal) == 1) {
          rms_voltage = adc_rms_voltage / jiga_watt_cal.voltage_scaler;
          rms_current = adc_rms_current / jiga_watt_cal.current_scaler;
          true_power = adc_true_power / jiga_watt_cal.power_scaler;
        }
        else {
          rms_voltage = 0;
          rms_current = 0;
          jiga_watt_cal.current_adc_offset = 0;
        }

        if (adc_rms_current <= jiga_watt_cal.current_adc_offset) {
          // disconnected state
          //adc_rms_current = 0;
          rms_current = 0;
          true_power = 0;
          apparent_power = 0;
        }

	energy = (float)adc_energy  / jiga_watt_cal.power_scaler / 60 / 60;
        apparent_power = rms_voltage * rms_current;
        //if (true_power > apparent_power)
        //  apparent_power = true_power;
        power_factor = true_power / apparent_power;

	printf( "Outlet %d: state=%d\n",outlet,outlet_state );
        printf ("RMS voltage = %f volts\n", rms_voltage);
        printf ("RMS current = %f amps\n", rms_current);
        printf ("true power = %f watts\n", true_power);
        printf ("appearent power = %f VA\n", apparent_power);
        printf ("power factor = %f VA\n", power_factor);
        printf ("adc_energy = %d\n", adc_energy);
        printf ("energy = %f WH\n", energy);
 

        sprintf (event_node, "%02x%02x%02x%02x", gw_pkt->subnet_mac[2],
                 gw_pkt->subnet_mac[1],
                 gw_pkt->subnet_mac[0], tran_pkt.mac_addr);


        check_and_create_node (event_node);
        val = reg_id_get (event_node, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg = create_sox_message ();
        msg_add_device_installation (msg, event_node, reg_id, "JigaWatt",
                                     "A Firefly Node with Power", time_str);

	
        sprintf (reg_name, "%s_VOLTAGE_%d", event_node,outlet);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        sprintf (sensor_type_str, "Voltage%d", outlet);
        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0001", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_rms_voltage);
        sprintf (sensor_adj_str, "%f", rms_voltage);
        msg_add_value_to_transducer (msg, event_node, "0001", sensor_adj_str, sensor_raw_str, time_str);

        sprintf (reg_name, "%s_CURRENT_%d", event_node,outlet);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        sprintf (sensor_type_str, "Current%d", outlet);
        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0002", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_rms_current);
        sprintf (sensor_adj_str, "%f", rms_current);
        msg_add_value_to_transducer (msg, event_node, "0002", sensor_adj_str, sensor_raw_str, time_str);

        sprintf (reg_name, "%s_POWER_%d", event_node,outlet);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        sprintf (sensor_type_str, "TruePower%d", outlet);
        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0003", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_true_power);
        sprintf (sensor_adj_str, "%f", true_power);
        msg_add_value_to_transducer (msg, event_node, "0003", sensor_adj_str, sensor_raw_str, time_str);

        sprintf (reg_name, "%s_APOWER_%d", event_node,outlet); val = reg_id_get (reg_name, reg_id); if (val == 0) strcpy (reg_id, "unknown");
        sprintf (sensor_type_str, "ApparentPower%d", outlet);
        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0004", reg_id, FALSE);
        sprintf (sensor_raw_str, "%f", apparent_power);
        sprintf (sensor_adj_str, "%f", apparent_power);
        msg_add_value_to_transducer (msg, event_node, "0004", sensor_adj_str, sensor_raw_str, time_str);

        sprintf (reg_name, "%s_POWER_FACTOR_%d", event_node,outlet); val = reg_id_get (reg_name, reg_id); if (val == 0) strcpy (reg_id, "unknown");
        sprintf (sensor_type_str, "PowerFactor%d", outlet);
        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0005", reg_id, FALSE);
        sprintf (sensor_raw_str, "%f", power_factor);
        sprintf (sensor_adj_str, "%f", power_factor);
        msg_add_value_to_transducer (msg, event_node, "0005", sensor_adj_str, sensor_raw_str, time_str);

        sprintf (reg_name, "%s_ENERGY_%d", event_node,outlet); val = reg_id_get (reg_name, reg_id); if (val == 0) strcpy (reg_id, "unknown");
        sprintf (sensor_type_str, "Energy%d", outlet);
        msg_add_transducer_installation (msg, event_node, sensor_type_str, "0006", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_energy);
        sprintf (sensor_adj_str, "%f", energy);
        msg_add_value_to_transducer (msg, event_node, "0006", sensor_adj_str, sensor_raw_str, time_str);


	if(xmpp_flag)
	{
        ret = publish_sox_message (connection, event_node, msg);
        delete_sox_message (msg);
        if (ret != XMPP_NO_ERROR) {
          //g_printerr ("Could not send packet for '%s'. Error='%s'\n",
          //            event_node, ERROR_MESSAGE (ret));
          sprintf (global_error_msg, "Could not publish Jiga Watt to %s: %s",
                   event_node, ERROR_MESSAGE (ret));
          log_write (global_error_msg);
          return -1;
        }
	}
	}
        break;


      case TRAN_POWER_SENSE_DEBUG:
        time (&timestamp);
        strftime (time_str, 100, "%Y-%m-%dT%X", localtime (&timestamp));
        printf ("Power Reply from 0x%x\n", tran_pkt.mac_addr);
        adc_rms_current = (tran_pkt.payload[0] << 8) | tran_pkt.payload[1];
        adc_rms_current2 = (tran_pkt.payload[13] << 8) | tran_pkt.payload[14];
        adc_rms_voltage = (tran_pkt.payload[30] << 8) | tran_pkt.payload[31];
        freq = tran_pkt.payload[32];
        adc_true_power =
          (tran_pkt.
           payload[2] << 16) | (tran_pkt.payload[3] << 8) |
          (tran_pkt.payload[4]);
        adc_true_power2 =
          (tran_pkt.
           payload[15] << 16) | (tran_pkt.payload[16] << 8) |
          (tran_pkt.payload[17]);
         adc_energy =
          (tran_pkt.
           payload[5] << 24) | (tran_pkt.payload[6] << 16) |
          (tran_pkt.payload[7] << 8) | (tran_pkt.payload[8]);
         adc_energy2 =
          (tran_pkt.
           payload[18] << 24) | (tran_pkt.payload[19] << 16) |
          (tran_pkt.payload[20] << 8) | (tran_pkt.payload[21]);
         adc_v_p2p_high = (tran_pkt.payload[26] << 8) | (tran_pkt.payload[27]);
        adc_v_p2p_low = (tran_pkt.payload[28] << 8) | (tran_pkt.payload[29]);
        adc_i_p2p_high = (tran_pkt.payload[9] << 8) | (tran_pkt.payload[10]);
        adc_i_p2p_low = (tran_pkt.payload[11] << 8) | (tran_pkt.payload[12]);
        adc_i_p2p_high2 = (tran_pkt.payload[22] << 8) | (tran_pkt.payload[23]);
        adc_i_p2p_low2 = (tran_pkt.payload[24] << 8) | (tran_pkt.payload[25]);
        time_secs =
          (tran_pkt.
           payload[33] << 24) | (tran_pkt.payload[34] << 16) |
          (tran_pkt.payload[35] << 8) | (tran_pkt.payload[36]);

        printf ("rms current adc = %d rms voltage adc = %d true power adc = %d\n",
           adc_rms_current, adc_rms_voltage, adc_true_power);
        printf ("p2p voltage adc low,center,high = %d,%d,%d\n", adc_v_p2p_low,
                (adc_v_p2p_low + adc_v_p2p_high) / 2, adc_v_p2p_high);
        printf ("p2p current adc low,center,high = %d,%d,%d\n", adc_i_p2p_low,
                (adc_i_p2p_low + adc_i_p2p_high) / 2, adc_i_p2p_high);
        printf ("freq = %d energy adc = %d time = %d seconds\n", freq,
                adc_energy, time_secs);


        printf ("rms current2 adc = %d rms voltage2 adc = %d true power2 adc = %d\n",
           adc_rms_current2, adc_rms_voltage, adc_true_power2);
        printf ("p2p current adc low,center,high = %d,%d,%d\n", adc_i_p2p_low2,
                (adc_i_p2p_low + adc_i_p2p_high) / 2, adc_i_p2p_high2);
        printf ("freq = %d energy adc = %d time = %d seconds\n", freq,
                adc_energy2, time_secs);

        float rms_voltage, rms_current, true_power, apparent_power,
          power_factor;

        float rms_current2, true_power2, apparent_power2,
          power_factor2;

        c_mac =
          gw_pkt->subnet_mac[2] << 24 | gw_pkt->
          subnet_mac[1] << 16 | gw_pkt->subnet_mac[0] << 8 | tran_pkt.
          mac_addr;

        if (jiga_watt_cal_get (c_mac,0, &jiga_watt_cal) == 1) {
          rms_voltage = adc_rms_voltage / jiga_watt_cal.voltage_scaler;
          rms_current = adc_rms_current / jiga_watt_cal.current_scaler;
          true_power = adc_true_power / jiga_watt_cal.power_scaler;
        }
        else {
          rms_voltage = 0;
          rms_current = 0;
          jiga_watt_cal.current_adc_offset = 0;
        }

        if (adc_rms_current <= jiga_watt_cal.current_adc_offset) {
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



	printf( "Outlet 0:\n" );
        printf ("RMS voltage = %f volts\n", rms_voltage);
        printf ("RMS current = %f amps\n", rms_current);
        printf ("true power = %f watts\n", true_power);
        printf ("appearent power = %f VA\n", apparent_power);
        printf ("power factor = %f VA\n", power_factor);
 
        if (jiga_watt_cal_get (c_mac,1, &jiga_watt_cal) == 1) {
          rms_current2 = adc_rms_current2 / jiga_watt_cal.current_scaler;
          true_power2 = adc_true_power2 / jiga_watt_cal.power_scaler;
        }
        else {
          rms_current2 = 0;
          jiga_watt_cal.current_adc_offset = 0;
        }

        if (adc_rms_current2 <= jiga_watt_cal.current_adc_offset) {
          // disconnected state
          //adc_rms_current = 0;
          rms_current2 = 0;
          true_power2 = 0;
          apparent_power2 = 0;
        }
        apparent_power2 = rms_voltage * rms_current2;
        power_factor2 = true_power2 / apparent_power2;


	printf( "\nOutlet 1:\n" );
        printf ("RMS voltage = %f volts\n", rms_voltage);
        printf ("RMS current = %f amps\n", rms_current2);
        printf ("true power = %f watts\n", true_power2);
        printf ("appearent power = %f VA\n", apparent_power2);
        printf ("power factor = %f VA\n", power_factor2);

        sprintf (event_node, "%02x%02x%02x%02x", gw_pkt->subnet_mac[2],
                 gw_pkt->subnet_mac[1],
                 gw_pkt->subnet_mac[0], tran_pkt.mac_addr);



        check_and_create_node (event_node);
        val = reg_id_get (event_node, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg = create_sox_message ();
        msg_add_device_installation (msg, event_node, reg_id, "JigaWatt",
                                     "A Firefly Node with Power", time_str);

        //sprintf( reg_name,"%s_BAT",event_node );
        //val=reg_id_get(reg_name,reg_id);
        //if(val==0) strcpy( reg_name, "unknown" );
        msg_add_transducer_installation (msg, event_node, "Voltage", "0001",
                                         reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_rms_voltage);
        sprintf (sensor_adj_str, "%f", rms_voltage);
        msg_add_value_to_transducer (msg, event_node, "0001", sensor_adj_str,
                                     sensor_raw_str, time_str);

        msg_add_transducer_installation (msg, event_node, "Current", "0002",
                                         reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_rms_current);
        sprintf (sensor_adj_str, "%f", rms_current);
        msg_add_value_to_transducer (msg, event_node, "0002", sensor_adj_str,
                                     sensor_raw_str, time_str);

        msg_add_transducer_installation (msg, event_node, "True Power",
                                         "0003", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", adc_true_power);
        sprintf (sensor_adj_str, "%f", true_power);
        msg_add_value_to_transducer (msg, event_node, "0003", sensor_adj_str,
                                     sensor_raw_str, time_str);


        msg_add_transducer_installation (msg, event_node, "Apparent Power",
                                         "0004", reg_id, FALSE);
        sprintf (sensor_raw_str, "%f", apparent_power);
        sprintf (sensor_adj_str, "%f", apparent_power);
        msg_add_value_to_transducer (msg, event_node, "0004", sensor_adj_str,
                                     sensor_raw_str, time_str);

        msg_add_transducer_installation (msg, event_node, "Power Factor",
                                         "0005", reg_id, FALSE);
        sprintf (sensor_raw_str, "%f", power_factor);
        sprintf (sensor_adj_str, "%f", power_factor);
        msg_add_value_to_transducer (msg, event_node, "0005", sensor_adj_str,
                                     sensor_raw_str, time_str);

	if(xmpp_flag)
	{
        ret = publish_sox_message (connection, event_node, msg);
        delete_sox_message (msg);
        if (ret != XMPP_NO_ERROR) {
          //g_printerr ("Could not send packet for '%s'. Error='%s'\n",
          //            event_node, ERROR_MESSAGE (ret));
          sprintf (global_error_msg, "Could not publish Jiga Watt to %s: %s",
                   event_node, ERROR_MESSAGE (ret));
          log_write (global_error_msg);
          return -1;
        }
	}

        break;

      case TRAN_FF_BASIC_SHORT:
        ff_basic_sensor_short_unpack (&tran_pkt, &sensor_short);
        c_mac =
          gw_pkt->subnet_mac[2] << 24 | gw_pkt->
          subnet_mac[1] << 16 | gw_pkt->subnet_mac[0] << 8 | tran_pkt.
          mac_addr;
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
        battery_cal = ((float) (sensor_short.battery + 100)) / 100;
        // Get local timestamp
        time (&timestamp);
        strftime (time_str, 100, "%Y-%m-%dT%X", localtime (&timestamp));
        printf ("Sensor pkt from 0x%x\n", tran_pkt.mac_addr);
        printf ("   Light: %u (%u)\n", light_cal, sensor_short.light);
        printf ("   Temperature: %f (%u)\n", temp_cal,
                sensor_short.temperature);
        printf ("   Acceleration: %u\n", sensor_short.acceleration);
        printf ("   Sound Level: %u\n", sensor_short.sound_level);
        printf ("   Battery: %f (%u)\n", battery_cal, sensor_short.battery);
        printf ("   Timestamp: %s\n", time_str);

        sprintf (event_node, "%02x%02x%02x%02x", gw_pkt->subnet_mac[2],
                 gw_pkt->subnet_mac[1],
                 gw_pkt->subnet_mac[0], tran_pkt.mac_addr);


        printf ("event node=%s\n", event_node);


        check_and_create_node (event_node);
        val = reg_id_get (event_node, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        printf ("    event node: %s reg_id: %s\n", event_node, reg_id);
        msg = create_sox_message ();

        msg_add_device_installation (msg, event_node, reg_id, "FIREFLY",
                                     "A Firefly Node", time_str);
        // Light
        sprintf (reg_name, "%s_LIGHT", event_node);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg_add_transducer_installation (msg, event_node, "Light", "0001",
                                         reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", sensor_short.light);
        sprintf (sensor_adj_str, "%d", light_cal);
        msg_add_value_to_transducer (msg, event_node, "0001", sensor_adj_str,
                                     sensor_raw_str, time_str);
        // Temperature
        sprintf (reg_name, "%s_TEMP", event_node);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg_add_transducer_installation (msg, event_node, "Temperature",
                                         "0002", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", sensor_short.light);
        sprintf (sensor_adj_str, "%2.2f", temp_cal);
        msg_add_value_to_transducer (msg, event_node, "0002", sensor_adj_str,
                                     sensor_raw_str, time_str);
        // Acceleration 
        sprintf (reg_name, "%s_ACCEL", event_node);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg_add_transducer_installation (msg, event_node, "Acceleration",
                                         "0003", reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", sensor_short.acceleration);
        sprintf (sensor_adj_str, "%d", sensor_short.acceleration);
        msg_add_value_to_transducer (msg, event_node, "0003", sensor_adj_str,
                                     sensor_raw_str, time_str);
        // Voltage 
        sprintf (reg_name, "%s_BAT", event_node);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg_add_transducer_installation (msg, event_node, "Voltage", "0004",
                                         reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", sensor_short.battery);
        sprintf (sensor_adj_str, "%1.2f", battery_cal);
        msg_add_value_to_transducer (msg, event_node, "0004", sensor_adj_str,
                                     sensor_raw_str, time_str);
        // Sound Level 
        sprintf (reg_name, "%s_AUDIO", event_node);
        val = reg_id_get (reg_name, reg_id);
        if (val == 0)
          strcpy (reg_id, "unknown");
        msg_add_transducer_installation (msg, event_node, "Audio", "0005",
                                         reg_id, FALSE);
        sprintf (sensor_raw_str, "%d", sensor_short.sound_level);
        sprintf (sensor_adj_str, "%d", sensor_short.sound_level);
        msg_add_value_to_transducer (msg, event_node, "0005", sensor_adj_str,
                                     sensor_raw_str, time_str);

        ret = publish_sox_message (connection, event_node, msg);
        delete_sox_message (msg);

        if (ret != XMPP_NO_ERROR) {
          sprintf (global_error_msg, "Transducer pkt could not send %s: %s",
                   event_node, ERROR_MESSAGE (ret));
          log_write (global_error_msg);
          return -1;
        }

        break;
      default:
        log_write ("Transducer pkt handler: unknown transducer pkt");
      }
    }
    break;
  default:
    sprintf (global_error_msg,
             "Transducer pkt handler: unknown pkt type (%d)",
             gw_pkt->pkt_type);
    log_write (global_error_msg);
  }
*/
