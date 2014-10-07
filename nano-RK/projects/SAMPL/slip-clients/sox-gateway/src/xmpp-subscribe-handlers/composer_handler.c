#include <composer_handler.h>
#include <loudmouth/loudmouth.h>
#include <expat.h>
#include <stdint.h>
#include <error_log.h>
#include <sampl.h>
#include <transducer_pkt.h>
#include <transducer_registry.h>
#include <globals.h>


#define MAC_MASK	0x1
#define KEY_MASK	0x2
#define VALUE_MASK	0x4

#define MAX_TRANS_MSGS		32

static int from_pubsub_node,trans_mac, trans_key, trans_value, trans_state;

//static TRANSDUCER_CMD_PKT_T t_tran_cmd_pkt;
//static TRANSDUCER_MSG_T t_tran_msg_pkts[MAX_TRANS_MSGS];
static SAMPL_DOWNSTREAM_PKT_T t_ds_pkt;
static uint8_t pkt_buf[MAX_PAYLOAD];
static uint8_t tran_msg_cnt;
static uint8_t system_pkt;
static char trans_name[64];


void send_transducer (trans_mac, trans_key, trans_value)
{
/*
  uint8_t l_buf[256], l_size;
  uint8_t i;
  printf ("Sending Transducer mac %d key: %d value: %d\n", trans_mac,
          trans_key, trans_value);
  t_ds_pkt.buf = pkt_buf;
  t_ds_pkt.payload_len = 0;
  t_ds_pkt.buf_len = DS_PAYLOAD_START;
  t_ds_pkt.payload_start = DS_PAYLOAD_START;
  t_ds_pkt.payload = &(pkt_buf[DS_PAYLOAD_START]);
  t_ds_pkt.pkt_type = TRANSDUCER_CMD_PKT;
  t_ds_pkt.ctrl_flags = DS_MASK | LINK_ACK | ENCRYPT;
  t_ds_pkt.ack_retry = 0xf;
  t_ds_pkt.last_hop_mac = 0;
  t_ds_pkt.hop_cnt = 0;
  t_ds_pkt.mac_filter_num = 0;
  t_ds_pkt.rssi_threshold = -32;
  t_ds_pkt.hop_max = 5;
  t_ds_pkt.delay_per_level = 0;
  t_ds_pkt.mac_check_rate = 100;
  t_tran_cmd_pkt.num_msgs = tran_msg_cnt;
  t_tran_cmd_pkt.msg = &t_tran_msg_pkts;

  t_ds_pkt.payload_len =
    	transducer_cmd_pkt_add (&t_tran_cmd_pkt, t_ds_pkt.payload);

  transducer_cmd_pkt_checksum (&t_tran_cmd_pkt, t_ds_pkt.payload);
  pack_downstream_packet (&t_ds_pkt);
  tx_q_add (t_ds_pkt.buf, t_ds_pkt.buf_len);
  tran_msg_cnt=0;
  system_pkt=0;
  trans_key=0;
  trans_value=0;
*/
}

void send_transducer_raw (char *value, int len)
{
  uint8_t l_buf[256], l_size;
  int i, j;
  char hex_digits[3];

  printf ("Transducer Raw Value: %s\n", value);
  if (len % 2 != 0) {
    printf ("Hex string not an even num of digits!\n");
    return;
  }
  for (j = 0; j < len / 2; j++) {
    hex_digits[0] = value[j * 2];
    hex_digits[1] = value[(j * 2) + 1];
    hex_digits[2] = '\0';
    sscanf (hex_digits, "%x", &l_buf[j]);
  }
  l_size = len / 2;
  tx_q_add (l_buf, l_size,0);



}

//XMLParser func called whenever the start of an XML element is encountered
static void XMLCALL startElement (void *data, const char *element_name,
                                  const char **attr)
{
  int i, j;
  unsigned int sensor_value = 0;
  char len, hex_digits[3];

/*
  if (strcmp (element_name, "items") == 0) {
    trans_state = 0;
    for (i = 0; attr[i]; i += 2) {
      const char *attr_name = attr[i];
      const char *attr_value = attr[i + 1];
      if (strcmp (attr_name, "node") == 0) {
        sscanf (attr_value, "%x", &from_pubsub_node);
	printf( "message from %x\n",from_pubsub_node);
      }

    }
  }




  // Find the MAC address of the device
  if (strcmp (element_name, "DeviceInstallation") == 0) {
    trans_state = 0;
    for (i = 0; attr[i]; i += 2) {
      const char *attr_name = attr[i];
      const char *attr_value = attr[i + 1];
      if (strcmp (attr_name, "id") == 0) {
        sscanf (attr_value, "%x", &trans_mac);
	if(trans_mac!=from_pubsub_node && system_pkt!=1 )
		{
		printf( "Warning: event-node id being spoofed!\n" );
		}
	else {
        	trans_mac &= 0xff;
        	trans_state |= MAC_MASK;
	}
      }

    }
  }

  // Find the name of the tranducer installation type
  if (strcmp (element_name, "TransducerInstallation") == 0) {
    for (i = 0; attr[i]; i += 2) {
      const char *attr_name = attr[i];
      const char *attr_value = attr[i + 1];
      if (strcmp (attr_name, "name") == 0) {
	strcpy( trans_name, attr_value );
      }

    }
  }

  // Fill in the actualy name, value pairs for the transducer packet
  if (strcmp (element_name, "TransducerCommand") == 0) {
    for (i = 0; attr[i]; i += 2) {
      const char *attr_name = attr[i];
      const char *attr_value = attr[i + 1];
      if (strcmp (attr_name, "rawvalue") == 0) {
        send_transducer_raw (attr[i + 1], strlen (attr[i + 1]));
      }

      if(strcmp(trans_name, "LedBlink")==0)
      {
      if (strcmp (attr_name, "name") == 0) {
	trans_value=0;
	if (strstr(attr_value, "red") != 0) trans_value|=TRAN_RED_LED_MASK;
      	if (strstr(attr_value, "green") != 0) trans_value|=TRAN_GREEN_LED_MASK;
      	if (strstr(attr_value, "blue") != 0) trans_value|=TRAN_BLUE_LED_MASK;
      	if (strstr(attr_value, "orange") != 0) trans_value|=TRAN_ORANGE_LED_MASK;
      	trans_key=TRAN_LED_BLINK;
	trans_state |= VALUE_MASK; 
	trans_state |= KEY_MASK; 
      }
      }

     if( strcmp(trans_name, "JigaWatt")==0 )
     {

      if (strcmp (attr_name, "name") == 0) {
        if (strcmp (attr_value, "socket0") == 0) {
          trans_key = TRAN_POWER_CTRL_SOCK;
          trans_value = 0;
          trans_state |= KEY_MASK;
        }
        if (strcmp (attr_value, "socket1") == 0) {
          trans_key = TRAN_POWER_CTRL_SOCK;
          trans_value = 1;
          trans_state |= KEY_MASK;
        }
        if (strcmp (attr_value, "read") == 0) {
          trans_key = TRAN_POWER_SENSE;
          trans_state |= KEY_MASK;
        }
       if (strcmp (attr_value, "debug") == 0) {
          trans_key = TRAN_POWER_SENSE_DEBUG;
          trans_value = 0;
          trans_state |= KEY_MASK;
          trans_state |= VALUE_MASK;
        }

      }

      if (strcmp (attr_name, "value") == 0) {
        if (strcmp (attr_value, "off") == 0) {
          trans_value |= SOCKET_OFF<<4;
          trans_state |= VALUE_MASK;
        }
        if (strcmp (attr_value, "on") == 0) {
          trans_value |= SOCKET_ON<<4;
          trans_state |= VALUE_MASK;
        }
        if (strcmp (attr_value, "socket0") == 0) {
          trans_value = 0;
          trans_state |= VALUE_MASK;
        }
        if (strcmp (attr_value, "socket1") == 0) {
          trans_value = 1;
          trans_state |= VALUE_MASK;
        }


      }
     }


    }

  }

*/
/*
  if (trans_state == (MAC_MASK | KEY_MASK | VALUE_MASK)) {
    // add parameters to transducer list 

  }
*/
}

//XMLParser func called whenever an end of element is encountered
static void XMLCALL endElement (void *data, const char *element_name)
{
/*
  if (strcmp (element_name, "DeviceInstallation") == 0) {
// increase transducer list or build final packet
    if (trans_state == (MAC_MASK | KEY_MASK | VALUE_MASK)) {
      // build and send single pkt
 	t_tran_msg_pkts[tran_msg_cnt].mac_addr = trans_mac;
  	t_tran_msg_pkts[tran_msg_cnt].key = trans_key;
  	t_tran_msg_pkts[tran_msg_cnt].value = trans_value;
        tran_msg_cnt++;
	if(system_pkt==0)
      		send_transducer (trans_mac, trans_key, trans_value);
        trans_state = 0;
    }
  }

  if (strcmp (element_name, "System") == 0) {
      if(system_pkt==1) send_transducer (trans_mac, trans_key, trans_value);
  }

*/
}

static void charData (void *userData, const char *s, int len)
{
  //if (len > 0)
  //  printf ("data: %s\n", s);
}

/*
void charData (void *userData, const XML_Char *s, int len) {

   XML_Parser ** parent;

   parent = (XML **) userData;
   xml_append (*parent, xml_createtextlen ((char *) s, len));
if(len>0)
printf( "data: %s\n",s );
}*/


void handle_inbound_xmpp_msgs (LmMessage * message)
{
  LmMessageNode *child = NULL;
  XML_Parser p;
  gchar *buf;

/*  child = lm_message_node_find_child(message->node,"event");
  if(child == NULL)
  {
    printf( "child is null\n" );
    return;
  }
*/
  child = lm_message_get_node (message);

  buf = lm_message_node_to_string (child);

  //printf( "buf=%s\n",buf ); 

  //Creates an instance of the XML Parser to parse the event packet
  p = XML_ParserCreate (NULL);
  if (!p) {
    log_write ("Couldnt allocate memory for XML Parser\n");
    return;
  }
  //Sets the handlers to call when parsing the start and end of an XML element
  XML_SetElementHandler (p, startElement, endElement);
  //XML_SetCharacterDataHandler(p, charData);  

  if (XML_Parse (p, buf, strlen (buf), 1) == XML_STATUS_ERROR) {
    sprintf (global_error_msg, "Parse Error at line %u:\n%s\n",
             XML_GetCurrentLineNumber (p),
             XML_ErrorString (XML_GetErrorCode (p)));
    log_write (global_error_msg);
    XML_ParserFree (p);
    return;
  }

  //lm_message_unref(child);
  g_free (buf);
  XML_ParserFree (p);


}
