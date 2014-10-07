// tdma_scheduler.h
// Contains graph coloring and schedule establishment

#include <tdma_asap.h>

#ifndef TDMA_SCHEDULER_H
#define TDMA_SCHEDULER_H

#define COLOR_SLOT_STRIDE 1

#define DIM 20
// Master: to color graph must know depth of tree
uint8_t depth_of_tree;

// Typedefs
typedef uint16_t node_addr_t;
typedef int16_t tdma_slot_t;
typedef int8_t tdma_color_t;

typedef enum{
    PKT_TYPE=0,
    DEST_ADDR=1,
    // dest addr is 2 bytes
    MY_LEVEL=3,
    SIZE_OF_CHILDREN=4,
    MY_ADDR=5,
    // ...
    MY_NO_CHILDREN=7,
    CHILDREN_INFO=8,
} tdma_notify_pkt_t;

typedef struct
{
  uint8_t level;
  uint8_t isDead;
  tdma_color_t color;
#ifdef COLOR_OPT
  tdma_color_t color2;
  tdma_color_t color4;
  uint8_t schedWithDiffLevel;
  uint8_t schedWithDiffLevel4;
#endif
  //uint8_t ableToSteal[MAX_SIBLINGS];
  node_addr_t address;
  node_addr_t parent;
  uint8_t totalChildren;
  node_addr_t children[TREE_MAX_CHILDREN];
} sensorInfo;

#ifdef TDMA_MASTER_MODE

uint16_t uncolored_node_list[2][DIM];  //List of nodes that have not been colored yet.
uint16_t colored_node_list[DIM];  //List of nodes that have  been colored
uint8_t Neighbors[DIM][DIM]; //To keep track of neighbors for graph coloring
uint8_t colors_used[DIM];	//To record the num of colors used per level
uint8_t colors_used_level[DIM][DIM];

    #ifdef COLOR_OPT
// for OPT2
uint16_t uncolored_node_list2[2][DIM];  //List of nodes that have not been colored yet.
uint16_t colored_node_list2[DIM];  //List of nodes that have  been colored
uint8_t colors_used2[DIM];	//To record the num of colors used per level
uint8_t colors_used_level2[DIM][DIM];

// for OPT4
uint16_t uncolored_node_list4[2][DIM];  //List of nodes that have not been colored yet.
uint16_t colored_node_list4[DIM];  //List of nodes that have  been colored
uint8_t colors_used4[DIM];	//To record the num of colors used per level
uint8_t colors_used_level4[DIM][DIM];
    #endif
#endif

sensorInfo sensorsInfo[DIM];

uint8_t graphColors[20][5];
//char graphColors[20][30];

// type to determine whether a slot is intended to
// send data to parent (data) or child (time sync)
// or RX
typedef enum {
    TDMA_RX,
    TDMA_TX_PARENT,
    TDMA_TX_CHILD
} tdma_slot_type_t;

typedef struct {
    tdma_slot_t slot;
    tdma_slot_type_t type;
    int8_t priority;
} tdma_schedule_entry_t;

// tree creation buffers
#define TREE_BUF_SIZE 115

RF_TX_INFO tree_rfTxInfo;
RF_RX_INFO tree_rfRxInfo;

// Tree creation buffers
uint8_t tree_tx_buf[TREE_BUF_SIZE];
uint8_t tree_rx_buf[TREE_BUF_SIZE];

#define TDMA_SCHEDULE_MAX_SIZE		64

// size of the subtree info array, in bytes
#define SUBTREE_INFO_SIZE 115
#define CHILD_INFO_SIZE 115

// how many times to push packets repeatedly to hopefully
// ensure delivery
#define CSMA_SEND_REPEAT            10
#define TREE_BACKOFF_TIME_MS        100

// Time to wait for children to respond to me total
// while making the tree
#define TREE_WAIT_TIME_MS      10000

// Predefined TDMA control pkts
#define TREE_MAKE   0xFF
#define TREE_NOTIFY 0xFE
#define TREE_SCHEDULE 0xFD

// my parent
node_addr_t my_parent;
uint8_t my_level;

void tdma_tree_init(uint8_t chan);
int8_t tdma_tree_make_master();
int8_t tdma_tree_make_slave();

void tdma_schedule_print();
int8_t nrk_time_cmp(nrk_time_t t1, nrk_time_t t2);

node_addr_t child_addrs[TREE_MAX_CHILDREN];
uint8_t num_children;

tdma_schedule_entry_t tdma_schedule[TDMA_SCHEDULE_MAX_SIZE] ;
uint8_t tdma_schedule_size;
uint8_t cur_schedule_entry;

void color_graph();
void discrete_color();

void obtain_schedule();
int8_t tdma_schedule_add(tdma_slot_t slot, tdma_slot_type_t type, int8_t priority);
tdma_schedule_entry_t tdma_schedule_get_next(tdma_slot_t slot);

#endif
