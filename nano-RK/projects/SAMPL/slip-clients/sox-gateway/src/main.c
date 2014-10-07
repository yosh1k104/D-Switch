#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sampl.h>
#include <slipstream.h>
#include <globals.h>
#include <ff_basic_sensor_cal.h>
#include <jiga_watt_cal.h>
#include <expat.h>
#include <tx_queue.h>
#include <error_log.h>
#include <xml_pkt_parser.h>
#include <errno.h>
#include <slip-server.h>

#if SQLITE_SUPPORT
	#include <db_transducer.h>
	#include <sqlite3.h>
#endif

#if SOX_SUPPORT
	#include <node_cache.h>
	#include <composer_handler.h>
	#include <soxlib.h>
	#include <xmpp_proxy.h>
	#include <xmpp_transducer.h>
	#include <xmpp_ping.h>
	#include <xmpp_nlist.h>
	#include <xmpp_pkt_writer.h>
	#include <loudmouth/loudmouth.h>
#endif


#define SEQ_CACHE_SIZE	24

#define IGNORE_PACKET	0
#define US_PACKET	1
#define P2P_PACKET	2

static time_t clockt;
static uint32_t epocht;

#define NONBLOCKING  0
#define BLOCKING     1

#define HEX_STR_SIZE	5

static char password[64];
static char xmpp_server[64];
static char xmpp_ssl_fingerprint[64];
static char pubsub_server[64];
static char username[64];
static uint32_t xmpp_server_port;

static uint8_t mirror_client_enabled;
static int mirror_client_port;
static char mirror_client_hostname[256];

void handle_incomming_pkt (uint8_t * rx_buf, uint8_t len);
void error (char *msg);
//void check_and_create_node(char *node_name);

static uint8_t slip_mirror_buf[128];

static char sampl_file_name[128];
static FILE *fp;

void seq_num_cache_init ();
int seq_num_cache_check (uint8_t * mac_addr, uint8_t seq_num,
                         uint8_t pkt_type);

typedef struct seq_num_cache {
  uint8_t addr[4];
  uint8_t seq_num;
  uint8_t pkt_type;
  int valid;
} seq_num_cache_t;

seq_num_cache_t seq_cache[SEQ_CACHE_SIZE];

SAMPL_DOWNSTREAM_PKT_T ds_pkt;

static char slip_server[MAX_BUF];
static uint32_t slip_port;
static char buf[1024];

static uint8_t seq_num;

#if SOX_SUPPORT
void load_subscriptions (char *file)
{
  FILE *fg;
  char event_node[128];
  int v, ret;

  printf ("Loading subscriptions from %s\n", file);
  fg = fopen (file, "r");
  if (fg == NULL) {
    printf ("Error, could not open %s subscriptions file\n");
    return;
  }


  
   sprintf( event_node, "%02x%02x%02x%02x",gw_subnet_2, gw_subnet_1, gw_subnet_0, gw_mac ); 
   printf ("Subscribing to event node %s (Gateway)\n", event_node);
      if ((ret = subscribe_to_node (connection, event_node)) != XMPP_NO_ERROR)
        g_printerr ("Could not subscribe to Gateway event node %s: %s\n", event_node,
                    ERROR_MESSAGE (ret));

  do {
    v = fscanf (fg, "%[^\n]\n", event_node);
    if (v != -1 && event_node[0] != '#') {
      printf ("Subscribing to event node %s\n", event_node);
      if ((ret = subscribe_to_node (connection, event_node)) != XMPP_NO_ERROR)
        g_printerr ("Could not subscribe to node %s: %s\n", event_node,
                    ERROR_MESSAGE (ret));
    }
  } while (v != -1);
  fclose (fg);
}
#endif

// Send an asynchronous message from XMPP to sensor network
int tx_msg ()
{
  int8_t v, i;
  uint8_t l_buf[128];
  uint8_t r_buf[128];
  uint8_t l_size;
  int echo;

  l_size = tx_q_get (l_buf);

  // Lets go in and fix the automatic transmit time parameters
  l_buf[SUBNET_MAC_2] = gw_subnet_2;
  l_buf[SUBNET_MAC_1] = gw_subnet_1;
  l_buf[SUBNET_MAC_0] = gw_subnet_0;
  //l_buf[SEQ_NUM] = 0;           // use gateway auto-cnt

  //only modify with time if DS pkt
  if((l_buf[CTRL_FLAGS] & DS_MASK) != 0)
  {
    l_buf[DS_LAST_HOP_MAC] = gw_mac;
    l_buf[DS_AES_CTR_0] = seq_num;
    if (l_buf[DS_AES_CTR_0] == 255)
      l_buf[DS_AES_CTR_1]++;
    if (l_buf[DS_AES_CTR_1] == 255)
      l_buf[DS_AES_CTR_2]++;
    if (l_buf[DS_AES_CTR_2] == 255)
      l_buf[DS_AES_CTR_3]++;
    clockt = time (NULL);
    epocht = clockt;
    l_buf[DS_EPOCH_TIME_0] = epocht & 0xff;       // Epoch time 
    l_buf[DS_EPOCH_TIME_1] = (epocht >> 8) & 0xff;
    l_buf[DS_EPOCH_TIME_2] = (epocht >> 16) & 0xff;
    l_buf[DS_EPOCH_TIME_3] = (epocht >> 24) & 0xff;
  }

  echo = 0;
  do {
    uint8_t *tbuf;
    v = slipstream_send (l_buf, l_size);
    usleep (100000);
    v = slipstream_receive (r_buf);
    if (v > 0) {
      printf( "Seq Num: %d\n",r_buf[SEQ_NUM] );
      if (echo == 0) {
        int ef;
        ef = 0;
        for (i = 0; i < l_size; i++)
          if (r_buf[i] != l_buf[i]) {
            ef = 1;
            if (debug_txt_flag == 1)
              printf ("Error %d:  %d!=%d\n", i, l_buf[i], r_buf[i]);
            log_write ("Invalid SLIP Packet Returned from Gateway");
          }
        if (ef == 0) {
          if (debug_txt_flag == 1)
            printf ("Gateway packet returned correctly.\n");
        }
        echo = 1;
      }
    }
    usleep (300000);
  } while (echo == 0);


  return 1;
}

#if SOX_SUPPORT
	void *main_publish_loop (gpointer data)
#else
	void main_loop()
#endif
{
  uint8_t num_script_pkts, script_index;
  uint8_t tx_buf[MAX_BUF];
  char xml_config_file_buf[MAX_XML_FILE];
  uint8_t rx_buf[MAX_BUF];
  int32_t v, i, len;
  uint8_t nav_time_secs;
  uint8_t reply_time_secs, echo;
  int32_t tmp;
  time_t reply_timeout, nav_timeout, t;
  uint8_t cmd_ready, error, ret;
  char token[64];
  char name[64];
  int slip_drop_cnt;
  int xml_buf_size;
  GW_SCRIPT_PKT_T gw_pkts[32];

  log_init ();


#if SOX_SUPPORT
  if (xmpp_flag) {
    connection = start_xmpp_client (username,
                                    password,
                                    xmpp_server,
                                    xmpp_server_port,
                                    xmpp_ssl_fingerprint,
                                    pubsub_server, handle_inbound_xmpp_msgs);

    if (connection == NULL) {
      g_printerr ("Could not start client.\n");
      return -1;
    }
  }

  printf ("Adding gateway node\n");
  if (xmpp_flag == 1) {
    //sscanf(username,"%[^@]",name);
    sprintf (name, "%02x%02x%02x%02x", gw_subnet_2, gw_subnet_1, gw_subnet_0,
             gw_mac);
    node_list_add (name);
  }

  if (xmpp_flag == 1) {
    // generate parent node for gateway
    ret = create_event_node (connection, name, NULL, FALSE);
    if (ret != XMPP_NO_ERROR) {
      if (ret == XMPP_ERROR_NODE_EXISTS)
	{
        if (debug_txt_flag)
          printf ("Node '%s' already exists\n", name);
        } else {
          g_printerr ("Could not create gateway event node '%s'. Error='%s'\n", name,
                      ERROR_MESSAGE (ret));
          return -1;
        }
    }
    else if (debug_txt_flag)
      printf ("Created event node '%s'\n", name);

  }

  load_subscriptions (subscribe_file_name);


  v = reg_id_load_from_file (name);
#endif

  v = slipstream_open (slip_server, slip_port, NONBLOCKING);

if(mirror_client_enabled)
  slipstream_server_open (mirror_client_port,mirror_client_hostname, slip_debug_flag);

  seq_num = 0;

  slip_drop_cnt = 0;
  xml_buf_size = load_xml_file (sampl_file_name, xml_config_file_buf);
  if (xml_buf_size == -1) {
    printf ("error loading config xml file: %s\n", sampl_file_name);
    exit (0);
  }

  // build packet from xml file
  num_script_pkts =
    build_sampl_pkts_from_xml (&gw_pkts, 32, xml_config_file_buf,
                               xml_buf_size);
  printf ("XML script returned: %d pkts\n", num_script_pkts);

  script_index = 0;
  while (1) {
    error = 0;
    cmd_ready = 0;
    
#if SOX_SUPPORT
    proxy_cleanup ();
#endif

    // Load next packet from script to send
    if (gw_pkts[script_index].type == DS_PKT) {

      ds_pkt.buf = gw_pkts[script_index].pkt;
      ds_pkt.buf_len = gw_pkts[script_index].size;
      unpack_downstream_packet (&ds_pkt, 0);

      if (debug_txt_flag == 1)
        print_ds_packet (&ds_pkt);

      for (i = 0; i < ds_pkt.buf_len; i++)
        tx_buf[i] = ds_pkt.buf[i];
      len = ds_pkt.buf_len;


      // Lets go in and fix the automatic transmit time parameters
      tx_buf[SUBNET_MAC_2] = gw_subnet_2;
      tx_buf[SUBNET_MAC_1] = gw_subnet_1;
      tx_buf[SUBNET_MAC_0] = gw_subnet_0;
      tx_buf[DS_LAST_HOP_MAC] = gw_mac;
      tx_buf[SEQ_NUM] = 0;      // use gateway auto-cnt
      tx_buf[DS_AES_CTR_0] = seq_num;
      if (tx_buf[DS_AES_CTR_0] == 255)
        tx_buf[DS_AES_CTR_1]++;
      if (tx_buf[DS_AES_CTR_1] == 255)
        tx_buf[DS_AES_CTR_2]++;
      if (tx_buf[DS_AES_CTR_2] == 255)
        tx_buf[DS_AES_CTR_3]++;
      clockt = time (NULL);
      epocht = clockt;
      tx_buf[DS_EPOCH_TIME_0] = epocht & 0xff;  // Epoch time 
      tx_buf[DS_EPOCH_TIME_1] = (epocht >> 8) & 0xff;
      tx_buf[DS_EPOCH_TIME_2] = (epocht >> 16) & 0xff;
      tx_buf[DS_EPOCH_TIME_3] = (epocht >> 24) & 0xff;


      seq_num++;
      nav_time_secs = tx_buf[DS_NAV];
      reply_time_secs = tx_buf[DS_DELAY_PER_LEVEL] * tx_buf[DS_HOP_MAX];


    retry:
      // Read Data packet from script file

      // Send the packet
      if (len > 128)
        len = 128;
      if (error == 0)
        v = slipstream_send (tx_buf, len);
      if (debug_txt_flag == 1) {
        if (debug_txt_flag == 1) {
          if (v == 0)
            printf ("Error sending\n");
          else
            printf ("Sent request %d\n", tx_buf[SEQ_NUM]);
        }
      }

      echo = 0;
      t = time (NULL);
      reply_timeout = t + reply_time_secs + 3;
      nav_timeout = t + nav_time_secs;

      // Collect Reply packets 
      while (reply_timeout > time (NULL)) {
        v = slipstream_receive (rx_buf);
        if (v > 0) {
          if (echo == 0) {
            int ef;
            ef = 0;
            for (i = 0; i < len; i++)
              if (rx_buf[i] != tx_buf[i]) {
                ef = 1;
                if (debug_txt_flag == 1)
                  printf ("Error %d:  %d!=%d\n", i, tx_buf[i], rx_buf[i]);
                log_write ("Invalid SLIP Packet Returned from Gateway");
              }
            if (ef == 0) {
              if (debug_txt_flag == 1)
                printf ("Gateway packet returned correctly.\n");
            }
            //else exit(0);
            echo = 1;
          }
          else
            handle_incomming_pkt (rx_buf, v);
        }
        usleep (1000);

#if SOX_SUPPORT
        proxy_cleanup ();
#endif

	if(mirror_client_enabled)
     	{
      		v=slipstream_server_non_blocking_rx (slip_mirror_buf);
      		if(v!=0) { 
			// If it is a P2P packet, add at a higher priority and send, otherwise just add
			if(((slip_mirror_buf[CTRL_FLAGS]&DS_MASK) ==0) && ((slip_mirror_buf[CTRL_FLAGS]&US_MASK)==0) )
				{
					tx_q_add(slip_mirror_buf,v,1); 
					v = tx_msg ();
				} else tx_q_add(slip_mirror_buf,v,0); 
			}
     	}

	}

      if (echo == 0) {
        slip_drop_cnt++;
        if (slip_drop_cnt > 3)
          log_write ("No SLIP reply from gateway 3 times");
        if (debug_txt_flag == 1)
          printf ("Error, no serial reply from gateway!\n");
        goto retry;
      }
      else
        slip_drop_cnt = 0;
      if (debug_txt_flag == 1)
        printf ("reply wait timeout...\n");


    }
    else if (gw_pkts[script_index].type == SLEEP) {
      printf ("Sleep Packet: %d\n", gw_pkts[script_index].nav);
      t = time (NULL);
      nav_timeout = t + gw_pkts[script_index].nav;
    }



    // What for NAV and service incomming messages 
    // This is the time window when the network is idle and can
    // be used for asynchronous communications.
    while (nav_timeout > time (NULL)) {
      v = slipstream_receive (rx_buf);
      if (v > 0) {
        handle_incomming_pkt (rx_buf, v);
        // Check if TX queue has data and send the request
      }
      if (tx_q_pending ()) {
        v = tx_msg ();
        nav_timeout += 2;
        sleep (1);
      }
      usleep (1000);
#if SOX_SUPPORT
      proxy_cleanup ();
#endif
if(mirror_client_enabled)
     {
      v=slipstream_server_non_blocking_rx (slip_mirror_buf);
      if(v!=0) 
	{
	if(((slip_mirror_buf[CTRL_FLAGS]&DS_MASK) ==0) && ((slip_mirror_buf[CTRL_FLAGS]&US_MASK)==0) )
		tx_q_add(slip_mirror_buf,v,1);
	else
		tx_q_add(slip_mirror_buf,v,0);
	}
     }
		 

    }

    script_index++;
    if (script_index == num_script_pkts)
      script_index = 0;
  }





}

void print_usage ()
{
  printf
    ("Usage: server port gateway_mac [-verbose] [-no_xmpp] [-enable_sqlite db-file-path] [-slip_debug] [-xmpp xmmp-config.txt] [-xml pkt-script.xml] [-reg registry.txt] [-sub subscribe-list.txt] [-slipstream_mirror port client-ip]\n");
  printf ("  gateway_mac e.g. 0x00000000\n");
  printf ("  verbose\tShow debugging messages\n");
  printf ("  no_xmpp\tDo not connect to XMPP server\n");
  printf ("  enable_sqlite\tDo not use local sqlite server\n");
  printf ("  slip_debug\tLog all SLIP packets to slip.log file\n");
  printf
    ("\nBy default, all config files are assumed to be in the \"configs\" directory.\n");
  printf ("All logs will be stored in the \"logs\" directory.\n");
  printf( "\nCompile Settings:\n" );

#if SOX_SUPPORT
   printf( "  SOX support enabled\n" );
#else
   printf( "  SOX support disabled\n" );
#endif


#if SQLITE_SUPPORT
   printf( "  SQLITE support enabled\n" );
#else
   printf( "  SQLITE support disabled\n" );
#endif


  exit (1);

}

int main (int argc, char *argv[])
{
#if SOX_SUPPORT
  GThread *main_thread = NULL;
  GError *error = NULL;
  GMainLoop *main_loop = NULL;
#endif

  uint32_t gw_mac_addr_full;
  char name[64];
  char xmpp_file_name[128];
  uint8_t param, i;
  int32_t v, ret;

//#if SQLITE_SUPPORT
//sqlite3 *db;
//v = sqlite3_open("testdb", &db);
//if( v ){
//    fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
//    sqlite3_close(db);
//    exit(1);
//  }
//#endif

  debug_txt_flag = 0;
  xmpp_flag = 1;
  db_flag = 0;
  slip_debug_flag = 0;
  print_input_flag = 0;
  mirror_client_enabled=0;
  
#if SOX_SUPPORT
  connection = NULL;
#endif


  if (argc < 4 || argc > 18) {
    print_usage ();
  }


  if (strlen (argv[3]) != 8 && strlen (argv[3]) != 10) {
    printf ("Invalid MAC address\n");
    print_usage ();
  }
  v = sscanf (argv[3], "%x", &gw_mac_addr_full);
  if (v != 1) {
    printf ("Invalid MAC address\n");
    print_usage ();
  }
  printf ("GW mac: 0x%08x\n", gw_mac_addr_full);
  gw_subnet_2 = gw_mac_addr_full >> 24;
  gw_subnet_1 = gw_mac_addr_full >> 16;
  gw_subnet_0 = gw_mac_addr_full >> 8;
  gw_mac = gw_mac_addr_full & 0xff;

  //set default files
  strcpy (xmpp_file_name, "configs/xmpp-config.txt");
  strcpy (sampl_file_name, "configs/pkt-script.xml");
  strcpy (subscribe_file_name, "configs/subscribe-nodes.txt");
  strcpy (registry_file_name, "configs/registry.txt");
  if (argc > 3) {
    for (i = 4; i < argc; i++) {
      // Grab dash command line options
      if (strcmp (argv[i], "-verbose") == 0) {
        printf ("Verbose Mode ON\n");
        debug_txt_flag = 1;
      }
      if (strcmp (argv[i], "-no_xmpp") == 0) {
        printf ("XMPP OFF\n");
        xmpp_flag = 0;
      }
      if (strcmp (argv[i], "-enable_sqlite") == 0) {
        printf ("SQLITE ENABLED: %s\n",argv[i+1]);
	init_db(argv[i+1]);
        db_flag = 1;
      }
       if (strcmp (argv[i], "-slip_debug") == 0) {
        printf ("SLIP DEBUG FLAG SET\n");
        slip_debug_flag = 1;
      }

      if (strcmp (argv[i], "-sub") == 0) {
        printf ("Loading subscription list: ");
        strcpy (subscribe_file_name, argv[i + 1]);
        printf ("%s\n", subscribe_file_name);
      }
      if (strcmp (argv[i], "-reg") == 0) {
        printf ("Loading registration file:");
        strcpy (registry_file_name, argv[i + 1]);
        printf ("%s\n", registry_file_name);
      }

      if (strcmp (argv[i], "-slipstream_mirror") == 0) {
	mirror_client_enabled=1;
        printf ("SLIP mirror client enabled:\n");
        mirror_client_port=atoi(argv[i + 1]);
	i++;
        strcpy(mirror_client_hostname, argv[i + 1]);
        printf ("  port %d\n",mirror_client_port );
        printf ("  host: %s\n",mirror_client_hostname);
      }
 
      if (strcmp (argv[i], "-xml") == 0) {
        printf ("Loading SAMPL xml config: ");
        strcpy (sampl_file_name, argv[i + 1]);
        printf ("%s\n", sampl_file_name);
      }
      if (strcmp (argv[i], "-xmpp") == 0) {
        printf ("Loading XMPP config: ");
        strcpy (xmpp_file_name, argv[i + 1]);
        printf ("%s\n", xmpp_file_name);
      }
    }
  }

#if SOX_SUPPORT
  if (xmpp_flag) {
    fp = fopen (xmpp_file_name, "r");
    if (fp == NULL) {
      printf ("XMPP config: No %s file!\n", xmpp_file_name);
      exit (0);
    }
    param = 0;
    do {
      v = fscanf (fp, "%[^\n]\n", buf);
      if (buf[0] != '#' && v != -1) {
        switch (param) {
        case 0:
          strcpy (username, buf);
          break;
        case 1:
          strcpy (password, buf);
          break;
        case 2:
          strcpy (xmpp_server, buf);
          break;
        case 3:
          xmpp_server_port = atoi (buf);
          break;
        case 4:
          strcpy (pubsub_server, buf);
          break;
        case 5:
          strcpy (xmpp_ssl_fingerprint, buf);
          break;
        }
        param++;
      }

    } while (v != -1 && param < 6);

    if (debug_txt_flag) {
      printf ("XMPP Client Configuration:\n");
      printf ("  username: %s\n", username);
      printf ("  password: %s\n", password);
      printf ("  xmpp server: %s\n", xmpp_server);
      printf ("  xmpp server port: %d\n", xmpp_server_port);
      printf ("  xmpp pubsub server: %s\n", pubsub_server);
      printf ("  xmpp ssl fingerprint: %s\n\n", xmpp_ssl_fingerprint);
    }
    if (param < 4) {
      printf
        ("Not enough xmpp configuration parameters in xmpp_config.txt\n");
      exit (0);
    }
    fclose (fp);
    if (debug_txt_flag)
      printf ("Initialized XMPP client\n");
  }

// pass server config information to the proxy server

  proxy_configure (xmpp_server, xmpp_server_port, pubsub_server,
                   xmpp_ssl_fingerprint);
#endif

  tx_q_init ();

  // FIXME: Make a file later to load all of the actuator devices
  // XXX: removed temporarily
//  if ((ret = subscribe_to_node (connection, "000003f0")) != XMPP_NO_ERROR)
//    g_printerr ("Could not subscribe to node : %s\n", ERROR_MESSAGE (ret));


  fp = fopen (sampl_file_name, "r");
  if (fp == NULL) {
    printf ("SAMPL config: No %s file!\n", sampl_file_name);
    printf ("This is required for sending control commands\n");
    exit (0);
  }

#if SOX_SUPPORT
  // clear list of nodes
  node_list_init ();
#endif

  seq_num_cache_init ();

  ff_basic_sensor_cal_load_params ("configs/ff_basic_sensor_cal.txt");
  jiga_watt_cal_load_params ("configs/jiga_watt_cal.txt");


  strcpy (slip_server, argv[1]);
  slip_port = atoi (argv[2]);


#if SOX_SUPPORT
  g_thread_init (NULL);
  set_sox_init ();

   //  main_loop = g_main_loop_new (NULL, FALSE);

  main_thread =
    g_thread_create ((GThreadFunc) main_publish_loop, connection, TRUE,
                     &error);
  if (error != NULL) {
    g_printerr ("Thread creation error: <%s>\n", error->message);
    return -1;
  }


  if (debug_txt_flag == 1)
    g_print ("created thread\n");
  g_thread_join (main_thread);
//g_main_loop_run (main_loop);
#else
  main_loop();
#endif


}

void handle_incomming_pkt (uint8_t * rx_buf, uint8_t len)
{
  int i, ret;
  int pkt_type;
  uint8_t mac[4];
  char node_name[64];
  SAMPL_GATEWAY_PKT_T gw_pkt;
  SAMPL_PEER_2_PEER_PKT_T p2p_pkt;
  if (debug_txt_flag == 1) {
    printf ("Incomming Pkt [%d] = ", len);
    for (i = 0; i < len; i++)
      printf ("%02x ", rx_buf[i]);
    printf ("\n");
  }

if(mirror_client_enabled)
  slipstream_server_tx (rx_buf, len);


  pkt_type = IGNORE_PACKET;

//  if ((rx_buf[CTRL_FLAGS] & US_MASK) != 0
//      && ((rx_buf[CTRL_FLAGS] & DS_MASK) != 0))
  if ((rx_buf[CTRL_FLAGS] & (US_MASK | DS_MASK)) == 0)
    pkt_type = P2P_PACKET;
  else if ((rx_buf[CTRL_FLAGS] & US_MASK) != 0
           && (rx_buf[CTRL_FLAGS] & DS_MASK) == 0)
    pkt_type = US_PACKET;
  else
    pkt_type = IGNORE_PACKET;

  mac[3] = rx_buf[SUBNET_MAC_2];
  mac[2] = rx_buf[SUBNET_MAC_1];
  mac[1] = rx_buf[SUBNET_MAC_0];
  mac[0] = rx_buf[GW_SRC_MAC];


  //printf ("checking: %02x%02x%02x%02x\n", mac[3], mac[2], mac[1], mac[0]);
// Check if it is a repeat packet
  if (seq_num_cache_check (mac, rx_buf[SEQ_NUM], rx_buf[PKT_TYPE]) == 1) {
    if (debug_txt_flag == 1)
      printf ("DUPLICATE PACKET!\n");
    sprintf (node_name, "%02x%02x%02x%02x", rx_buf[SUBNET_MAC_2],
             rx_buf[SUBNET_MAC_1], rx_buf[SUBNET_MAC_0], rx_buf[GW_SRC_MAC]);
    printf ("mac=%s seq_num=%d type=%d\n", node_name, rx_buf[SEQ_NUM],
            rx_buf[PKT_TYPE]);
  }
  else {
    // Create an event node if it doesn't already exist and if it is an infrastructure node

#if SOX_SUPPORT
    if (xmpp_flag == 1) {
      if (pkt_type != IGNORE_PACKET
          && (rx_buf[CTRL_FLAGS] & MOBILE_MASK) != 0) {
        sprintf (node_name, "%02x%02x%02x%02x", rx_buf[SUBNET_MAC_2],
                 rx_buf[SUBNET_MAC_1], rx_buf[SUBNET_MAC_0],
                 rx_buf[GW_LAST_HOP_MAC]);

        check_and_create_node (node_name);
      }

    }

#endif


// The only p2p packet that we understand from a mobile node is the XMPP_MSG
    if (pkt_type == P2P_PACKET && rx_buf[PKT_TYPE] != XMPP_PKT)
      pkt_type = IGNORE_PACKET;

    gw_pkt.buf = rx_buf;
    gw_pkt.buf_len = len;

if (debug_txt_flag == 1)
   print_gw_packet_elements(&gw_pkt);

    unpack_gateway_packet (&gw_pkt);



    if (debug_txt_flag == 1)
      printf ("Calling pkt handler for pkt_type %d\n", gw_pkt.pkt_type);
    if (gw_pkt.error_code != 0) {
      char my_str[100];
      sprintf (my_str, "Gw pkt error code = 0x%02x", gw_pkt.error_code);
      log_write (my_str);
    }

    switch (gw_pkt.pkt_type) {
    case PING_PKT:
    case ACK_PKT:
#if SOX_SUPPORT
      if(xmpp_flag) send_xmpp_ping_pkt (&gw_pkt);
#endif
      if (debug_txt_flag == 1)
        printf ("PING or ACK packet\n");
      break;

    case XMPP_PKT:
#if SOX_SUPPORT
      if(xmpp_flag) xmpp_pkt_handler (&gw_pkt);
#endif
      if (debug_txt_flag == 1)
        printf ("XMPP packet\n");
      break;


    case EXTENDED_NEIGHBOR_LIST_PKT:
#if SOX_SUPPORT
      if(xmpp_flag) extended_nlist_pkt_handler (&gw_pkt);
#endif
      if (debug_txt_flag == 1)
        printf ("Extended Neighbor List packet\n");
      break;

    case TRANSDUCER_PKT:


#if SQLITE_SUPPORT
      if(db_flag) db_write_transducer_pkt (&gw_pkt);
#endif

#if SOX_SUPPORT
      if(xmpp_flag) publish_xmpp_transducer_pkt (&gw_pkt);
#endif
      if (debug_txt_flag == 1)
        printf ("TRANSDUCER packet\n");
      break;

    case TRACEROUTE_PKT:
      if (debug_txt_flag == 1)
        printf ("TRACEROUTE packet\n");
      break;


    default:
      if (debug_txt_flag == 1)
        printf ("Unknown Packet\n");
    }
  }

}





void seq_num_cache_init ()
{
  int i;
  for (i = 0; i < SEQ_CACHE_SIZE; i++)
    seq_cache[i].valid = 0;
}


// This function returns 1 if the packet is a repeat and should be surpressed.
// It returns 0 if the packet is unique.
int seq_num_cache_check (uint8_t * mac_addr, uint8_t seq_num,
                         uint8_t pkt_type)
{
  int i, j;
  int match;

/*
for(i=0; i<SEQ_CACHE_SIZE; i++ )
	{
	if(seq_cache[i].valid!=0) printf( "cache %d: mac %02x%02x%02x%02x seq=%d type=%d ttl=%d\n",
	i, seq_cache[i].addr[3],seq_cache[i].addr[2],seq_cache[i].addr[1],
	seq_cache[i].addr[0],seq_cache[i].seq_num, seq_cache[i].pkt_type, seq_cache[i].valid );
	}
*/

// This is to stop caching SAMPL reply packets.
// Reply packets all come from the gateway with the same
// seq number and packet type
  if (mac_addr[0] == gw_mac &&
      mac_addr[1] == gw_subnet_0 &&
      mac_addr[2] == gw_subnet_1 && mac_addr[3] == gw_subnet_2)
    return 0;

  for (i = 0; i < SEQ_CACHE_SIZE; i++) {
    if (seq_cache[i].valid > 0) {
      seq_cache[i].valid--;
      if (mac_addr[0] == seq_cache[i].addr[0] &&
          mac_addr[1] == seq_cache[i].addr[1] &&
          mac_addr[2] == seq_cache[i].addr[2] &&
          mac_addr[3] == seq_cache[i].addr[3]) {
        seq_cache[i].valid = 100;
        // This is a repeat packet
        if (seq_num == seq_cache[i].seq_num
            && pkt_type == seq_cache[i].pkt_type) {
          return 1;

        }
        else {
          seq_cache[i].seq_num = seq_num;
          seq_cache[i].pkt_type = pkt_type;
          return 0;
        }
      }
    }

  }


  for (i = 0; i < SEQ_CACHE_SIZE; i++) {
    if (seq_cache[i].valid == 0) {
      seq_cache[i].addr[0] = mac_addr[0];
      seq_cache[i].addr[1] = mac_addr[1];
      seq_cache[i].addr[2] = mac_addr[2];
      seq_cache[i].addr[3] = mac_addr[3];
      seq_cache[i].seq_num = seq_num;
      seq_cache[i].pkt_type = pkt_type;
      seq_cache[i].valid = 100;
      return 0;
    }
  }

  return 0;
}




