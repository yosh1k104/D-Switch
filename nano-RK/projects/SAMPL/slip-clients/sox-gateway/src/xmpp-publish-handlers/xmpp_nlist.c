#include <stdlib.h>
#include <time.h>
#include <xmpp_nlist.h>
#include <sampl.h>
#include <soxlib.h>
#include <node_cache.h>
#include <globals.h>
#include <ack_pkt.h>
#include <neighbor_pkt.h>
#include <transducer_pkt.h>
#include <transducer_registry.h>
#include <xmpp_proxy.h>
#include <tx_queue.h>


static char buf[1024];

void extended_nlist_pkt_handler (SAMPL_GATEWAY_PKT_T * gw_pkt)
{
  char node_name[MAX_NODE_LEN];
  char publisher_node_name[MAX_NODE_LEN];
  uint8_t num_msgs, i;
  char timeStr[100];
  time_t timestamp;
  int8_t rssi, ret;
  uint8_t t;

  if (gw_pkt->payload_len == 0) {
    if (debug_txt_flag)
      printf ("Malformed packet!\n");
    return;
  }


  sprintf (publisher_node_name, "%02x%02x%02x%02x", gw_pkt->subnet_mac[2],
           gw_pkt->subnet_mac[1], gw_pkt->subnet_mac[0], gw_pkt->src_mac);


  if (debug_txt_flag == 1)
    printf ("Data for node: %s\n", publisher_node_name);
  // Check nodes and add them if need be
  if (xmpp_flag == 1)
    check_and_create_node (publisher_node_name);
  // publish XML data for node
  time (&timestamp);
  strftime (timeStr, 100, "%Y-%m-%d %X", localtime (&timestamp));

  sprintf (buf, "<Node id=\"%s\" type=\"FIREFLY\" timestamp=\"%s\">",
           publisher_node_name, timeStr);

  num_msgs = gw_pkt->payload[0];
  printf ("Extended Neighbor List %d:\n", num_msgs);
  if (gw_pkt->num_msgs > (MAX_PKT_PAYLOAD / NLIST_PKT_SIZE))
    return;
  for (i = 0; i < num_msgs; i++) {
    sprintf (node_name, "%02x%02x%02x%02x", gw_pkt->payload[1 + i * 5],
             gw_pkt->payload[1 + i * 5 + 1],
             gw_pkt->payload[1 + i * 5 + 2], gw_pkt->payload[1 + i * 5 + 3]);
    rssi = (int8_t) gw_pkt->payload[1 + i * 5 + 4];
    sprintf (&buf[strlen (buf)], "<Link linkNode=\"%s\" rssi=\"%d\"/>",
             node_name, rssi);
  }
  sprintf (&buf[strlen (buf)], "</Node>");

  if (debug_txt_flag == 1)
    printf ("Publish: %s\n", buf);
  if (xmpp_flag == 1)
    ret = publish_to_node (connection, publisher_node_name, buf);
  if (xmpp_flag && ret != XMPP_NO_ERROR)
    printf ("XMPP Error: %s\n", ERROR_MESSAGE (ret));


  if (debug_txt_flag == 1)
    printf ("Publish done\n");
}
