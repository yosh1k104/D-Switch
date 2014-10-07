/***********************************************
**  tdma_scheduler.c
************************************************
** Part of TDMA-ASAP, includes graph coloring and
** schedule decision functions.
**
** Author: John Yackovich
************************************************/

#include <include.h>
#include <ulib.h>
#include <stdlib.h>
#include <tdma_asap_scheduler.h>
#include <stdio.h>
#include <nrk.h>
#include <nrk_error.h>
#include <nrk_timer.h>
#include <avr/eeprom.h>
#include <nrk_eeprom.h>

// Print ALL SORTS of debug output
//#define TDMA_SCHED_DEBUG

// in tdma.c
extern uint8_t tdma_init_done;
extern int8_t _tdma_channel_check();
extern tdma_node_mode_t tdma_node_mode;


uint16_t tree_addr16;
uint8_t tree_channel;

uint8_t tree_init_done=0;
uint8_t tree_made=0;

uint8_t child_has_color;
uint8_t info_buf_size;

// A node's view of its part of the tree
uint8_t subtree_info[SUBTREE_INFO_SIZE];
uint8_t subtree_info_size;

// Master: to color graph must know depth of tree
uint8_t depth_of_tree;

// my children
uint8_t child_info[CHILD_INFO_SIZE];
uint8_t child_info_size;

// neighbors
node_addr_t neighbor_info[TREE_MAX_NEIGHBORS];
uint8_t num_neighbors;

// offset while parsing the child info
uint8_t child_info_offset;

nrk_time_t cur_time;
nrk_time_t backoff_time;

void tdma_tree_init(uint8_t chan)
{
    uint32_t mac;
    read_eeprom_mac_address(&mac);
    tree_addr16 = (mac & 0xFFFF);

    rf_set_cca_thresh(-45);
    rf_init(&tree_rfRxInfo, chan, 0xffff, tree_addr16);
    srand(tree_addr16);

    tree_channel = chan;
    num_children = 0;
    num_neighbors = 0;
    child_info_size = 0;
    subtree_info_size = 0;
    depth_of_tree = 0;
    
    child_info_offset = 0;
#ifdef TDMA_SCHED_DEBUG
    printf("CIO: %d\r\n", child_info_offset);
#endif

    for (int8_t i = 0; i < DIM; i++)
        sensorsInfo[i].isDead = 1;

    tree_init_done = 1;
}

uint8_t tdma_tree_made()
{
    return tree_made;
}
// Returns my level of the tree
uint8_t tdma_tree_level_get()
{
    if (tree_made == 0)
        return NRK_ERROR;

    return my_level;
}

uint8_t tdma_tree_is_leaf()
{
    if (tree_made == 0)
        return NRK_ERROR;

    return (num_children == 0);
}



/*******
 * Compares 2 nrk_time_t values, returns 1, 0, -1 for gt, eq, lt
 */
int8_t nrk_time_cmp(nrk_time_t time1, nrk_time_t time2)
{
    if (time1.secs > time2.secs)
        return 1;
    else if (time1.secs == time2.secs)
    {
        //secs are equal, check nanos
        if (time1.nano_secs > time2.nano_secs)
            return 1;
        else
            return -1;
    }
    else
    {
        //time is less
        return -1;
    }
}

// assign colors to:
// some set (1 or more) of TX slots specified to be to the parent (data)
// some set (1 or more) of TX slots to be to children  (time sync/ actuation)

tdma_slot_t get_slot_from_color(tdma_color_t color, tdma_slot_type_t type)
{
    if (type == TDMA_TX_PARENT)
    {
        // this is defined arbitrarily.  Right now it's just 2 * color
        // for send to parent slot and send to child slot is 2 * color + 1
        return (COLOR_SLOT_STRIDE*2)*color + COLOR_SLOT_STRIDE;
    }
    else if (type == TDMA_TX_CHILD)
    {
        return (COLOR_SLOT_STRIDE*2)*color;
    }
    else
    {
        return -1;
    }
}


int8_t tdma_schedule_add(tdma_slot_t slot, tdma_slot_type_t slot_type, int8_t priority)
{
    if (tdma_schedule_size >= TDMA_SCHEDULE_MAX_SIZE)
        return NRK_ERROR;

    tdma_schedule[tdma_schedule_size].slot = slot;
    tdma_schedule[tdma_schedule_size].type = slot_type;
    tdma_schedule[tdma_schedule_size].priority = priority;

#ifdef TDMA_SCHED_DEBUG
    printf("Added slot: %d , %d\r\n", slot, tdma_schedule[tdma_schedule_size].slot);
#endif
    tdma_schedule_size++;

    return NRK_OK; 
}

tdma_schedule_entry_t tdma_schedule_get_next(tdma_slot_t slot)
{

    int8_t smallest;
    int8_t smallest_larger;

    //get the smallest slot bigger than this one
    tdma_schedule_entry_t result;

    result = tdma_schedule[0];
    smallest = -1;
    smallest_larger = -1;

    for (int8_t i = 0; i < tdma_schedule_size; i++)
    {
        if ((smallest == -1) || tdma_schedule[i].slot < tdma_schedule[smallest].slot)
        {
            smallest = i;

        }
        if (slot < tdma_schedule[i].slot) /* ADDED NOT TO STAY  && tdma_schedule[i].priority <= 0)*/
        {
            if ((smallest_larger == -1 ) || tdma_schedule[i].slot < tdma_schedule[smallest_larger].slot)
                smallest_larger = i;
            //result =  tdma_schedule[i];
            //break;
        }
    }

    if (smallest_larger == -1)
        smallest_larger = smallest;

    //printf("getting entry %d\r\n", i);
    //tdma_schedule_entry_t entry = tdma_schedule[cur_schedule_entry];
    //cur_schedule_entry = (cur_schedule_entry + 1) % tdma_schedule_size;
    //printf("next slot: %d\r\n", tdma_schedule[smallest_larger].slot);

    //return result;
    return tdma_schedule[smallest_larger];
}

void tdma_schedule_print()
{
    // Go through tdma schedule, pruint8_t slots
    uint8_t cur_entry;
    nrk_kprintf(PSTR("MY TDMA SCHEDULE\r\n"));

    for (cur_entry = 0; cur_entry < tdma_schedule_size; cur_entry++)
    {
        printf("-slot %d, type %d, pr %d\r\n", 
                tdma_schedule[cur_entry].slot,
                tdma_schedule[cur_entry].type,
                tdma_schedule[cur_entry].priority);
    }

}

int8_t add_neighbor(node_addr_t neighbor_addr)
{
    for (uint8_t i = 0; i < num_neighbors; i++)
    {
        if (neighbor_info[i] == neighbor_addr)
            return NRK_ERROR;
    }
    if (num_neighbors < TREE_MAX_CHILDREN)
    {
        neighbor_info[num_neighbors++] = neighbor_addr;
        return NRK_OK;
    }

    return NRK_ERROR;
}


// Here, I have to add the child, but also know the size of the array
// they are sending me with their own child info

int8_t add_child(uint8_t * new_child, uint8_t new_child_size)
{
    //printf("in add child: %d %d\r\n", 
    //                                            new_child[0],
    //                                            new_child[1]);

    memcpy(&child_info[child_info_size], new_child, new_child_size);

    uint16_t child_num_print = child_info[child_info_size];
    child_num_print <<= 8;
    child_num_print |= child_info[child_info_size+1];

#ifdef TDMA_SCHED_DEBUG
    printf("ADDED CHILD %d, %d, %d\r\n", child_info[child_info_size],
        child_info[child_info_size+1], child_num_print);
#endif

    child_info_size += new_child_size;
    //num_children++;
    return NRK_OK;
}


int8_t add_child_to_list(node_addr_t child_addr)
{
    for (uint8_t i = 0; i < num_children; i++)
    {
        if (child_addrs[i] == child_addr)
            return NRK_ERROR;
    }
    if (num_children < TREE_MAX_CHILDREN)
    {
        child_addrs[num_children++] = child_addr;
        return NRK_OK;
    }

    return NRK_ERROR;

}


/*
int8_t build_subtree_array()
{
    memcpy(subtree_info[subtree_info_size], tree_addr16, ADDR_SIZE);
    subtree_info_size+= ADDR_SIZE;

    subtree_info[subtree_info_size++] = num_children;

    memcpy(subtree_info[subtree_info_size], child_info, (num_children * ADDR_SIZE));
    subtree_info_size += num_children * ADDR_SIZE;

    subtree_info[subtree_info_size++] = num_neighbors;
    memcpy(subtree_info[subtree_info_size], neighbor_info, (num_neighbors * ADDR_SIZE));
    subtree_info_size += num_neighbors * ADDR_SIZE;
}
*/

// Here we fill the info array (a tx buffer) with my address,
// and info about all children I've collected and neighbors

int8_t build_subtree_array(uint8_t * info_arr)
{
    // might want to do this with shifting instead

    info_arr[info_buf_size]   = (tree_addr16 >> 8);
    info_arr[info_buf_size+1] = (tree_addr16 & 0xFF);

    //memcpy(info_arr[info_buf_size], tree_addr16, ADDR_SIZE);

#ifdef TDMA_SCHED_DEBUG
    printf("my address to send: %d %d\r\n", info_arr[info_buf_size],
            info_arr[info_buf_size+1]);
#endif


    info_buf_size += ADDR_SIZE;

    info_arr[info_buf_size++] = num_children;

    memcpy(&info_arr[info_buf_size], child_info, child_info_size);
    info_buf_size += child_info_size;

    info_arr[info_buf_size++] = num_neighbors;

    // flip the dang bits
    for (uint8_t i = 0; i < num_neighbors; i++)
    {
        info_arr[info_buf_size] = neighbor_info[i] >> 8;
        info_arr[info_buf_size+1] = neighbor_info[i] & 0xFF;
        //memcpy(&info_arr[info_buf_size], neighbor_info, (num_neighbors * ADDR_SIZE));
        
        info_buf_size+=2;
    }

    return NRK_OK;
    //info_buf_size += num_neighbors * ADDR_SIZE;
}

// What needs done:
// Define MKTREE and TREE_NOTIFY in tdma.h
// handle the backoff for transmitting packets
//
// See if addr attributes work
// Loads of other fun stuff.
//


// Send out packet to everyone to become their parent
void send_tree_message()
{
    int8_t v;
    uint32_t b;
    nrk_time_t backoff_time;
    nrk_time_t cur_time;

    // send out a packet to everyone
    //tree_rfTxInfo.pPayload[0] = TREE_MAKE;
    //tree_rfTxInfo.pPayload[1] = my_level;
    tree_tx_buf[0] = TREE_MAKE;
    tree_tx_buf[1] = my_level;

    tree_rfTxInfo.pPayload = tree_tx_buf;
    //tree_rfTxInfo.length = TREE_TX_BUF_SIZE;
    tree_rfTxInfo.length = 2;

    // do things the bmac way sorta, back off for a random period
    // of time
    
    b=((uint32_t) TREE_BACKOFF_TIME_MS*1000)/(((uint32_t)rand()%100)+1);
    nrk_spin_wait_us(b);

    while (!(v = _tdma_channel_check()))
    {
        rf_rx_off();
        nrk_time_get(&cur_time);

        backoff_time.secs = TREE_BACKOFF_TIME_MS / 1000;
        backoff_time.nano_secs = (TREE_BACKOFF_TIME_MS % 1000) * NANOS_PER_MS;
        nrk_time_add(&backoff_time, backoff_time, cur_time);
        nrk_time_compact_nanos(&backoff_time);

        nrk_wait_until(backoff_time);
    }

    rf_rx_off();
    rf_tx_packet( &tree_rfTxInfo );

#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("Sent MKTREE\r\n"));
#endif
}

// Code to wait for children to notify me I am their parent
void wait_for_responses()
{
    uint8_t n;
    uint8_t expired = 0;
    node_addr_t child_addr;
    node_addr_t dest_addr = 0;
    uint8_t message_type;

#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("I wait for responses here.\r\n"));
#endif

    nrk_time_t wait_time;
    nrk_time_t cur_time;

    nrk_time_get(&cur_time);

    uint16_t my_wait_time_ms = TREE_WAIT_TIME_MS / (my_level+1);
    wait_time.secs = my_wait_time_ms / 1000;
    wait_time.nano_secs = (my_wait_time_ms % 1000) * NANOS_PER_MS;

    nrk_time_compact_nanos(&wait_time);
    nrk_time_add(&wait_time, wait_time, cur_time);
    nrk_time_compact_nanos(&wait_time);

    tree_rfRxInfo.max_length = TREE_BUF_SIZE;
    tree_rfRxInfo.pPayload = tree_rx_buf;

    rf_set_rx( &tree_rfRxInfo ,tree_channel);


    while (!expired)
    {
        rf_polling_rx_on();
        //while (rf_rx_check_fifop() == 0)
        //    ;
        while (((n=rf_polling_rx_packet()) == 0) && !expired)
        {
            nrk_time_get(&cur_time);
            if (nrk_time_cmp(cur_time, wait_time) >= 0)
                expired = 1;
        }

        //make sure a valid packet 
        if (n == 0)
        {
#ifdef TDMA_SCHED_DEBUG
            nrk_kprintf(PSTR("WFR: Timeout reached\r\n"));
#endif
            continue;
        }
        else if (n != 1)
        {
#ifdef TDMA_SCHED_DEBUG
            nrk_kprintf(PSTR("WFR: Invalid recv\r\n"));
#endif
            continue;
        }

        rf_rx_off();

        // packet is here, check for right type
        message_type = tree_rfRxInfo.pPayload[PKT_TYPE];
        //printf("wfr:msg type: %d\r\n",message_type);

        if (message_type != TREE_NOTIFY)
        {
            //nrk_kprintf(PSTR("  msg type wrong. \r\n"));
            continue;
        }

        // Extract the destination address.  If it's for me, I have to
        // do parental things (like take the children and forward them.
        // Otherwise, I add the address to my neighbor list and turn a blind eye.
        dest_addr = tree_rfRxInfo.pPayload[DEST_ADDR];
        dest_addr <<= 8;
        dest_addr |= tree_rfRxInfo.pPayload[DEST_ADDR+1];

        //printf("Dest addr : %d \r\n", dest_addr);
        if (dest_addr == tree_addr16)
        {
                
                // open the present! This child has sent
                // me all of its subtree information
                uint8_t child_pkt_size = tree_rfRxInfo.pPayload[SIZE_OF_CHILDREN];
                
                // add this child
                child_addr = tree_rfRxInfo.srcAddr;

                //extract 

                // unless I've heard from this child,
                // add it to my list
                if (add_child_to_list(child_addr) == NRK_OK)
                {
#ifdef TDMA_SCHED_DEBUG
                    printf("Adding child %d, %d %d rssi %d\r\n", child_addr, 
                                tree_rfRxInfo.pPayload[MY_ADDR],
                                tree_rfRxInfo.pPayload[MY_ADDR],
                                tree_rfRxInfo.rssi);
#endif

                    add_child_to_list(child_addr);
                    add_child(&tree_rfRxInfo.pPayload[MY_ADDR], child_pkt_size);
                    //add_child(child_addr);
                }
                else
                {
                    //nrk_kprintf(PSTR("Child already added.\r\n"));
                }
        }
        else
        {
#ifdef TDMA_SCHED_DEBUG
            printf("Adding neighbor %d\r\n", dest_addr);
#endif
            add_neighbor(dest_addr);
        }

        nrk_time_get(&cur_time);
        
        if (nrk_time_cmp(cur_time, wait_time) >= 0)
            expired = 1;
    }
    
}

// After I receive a packet, I want to notify the sender that
// I am now their child
// TODO: Include my children, and information provided by my children

void notify_parent()
{
    int8_t v;
    uint16_t b;
    nrk_time_t backoff_time;
    nrk_time_t cur_time;

    /* My packet will look something like this ....
     *  0   : Tree build packet
     *  1-2 : Destination address (parent)
     *  3   : My level
     *  4   : Size of the following information block:
     *  5-6 : My address
     *  7   : My number of children
     *  8...: My children addresses etc
     */
        
    tree_tx_buf[PKT_TYPE] = TREE_NOTIFY;
    //tree_tx_buf[1] will contain size...
    tree_tx_buf[MY_LEVEL] = my_level;

    info_buf_size = MY_ADDR; // so far...

    // put in my address
    // NEVERMIND: Done in build_subtree_array
    tree_tx_buf[DEST_ADDR] = (my_parent >> 8);
    tree_tx_buf[DEST_ADDR+1] = (my_parent & 0xFF);

    // FIXME Bad practice here, should count from start

    // add my address, the children I've collected, and my neighbors
    build_subtree_array(tree_tx_buf);

    tree_tx_buf[SIZE_OF_CHILDREN] = info_buf_size - MY_ADDR;

#ifdef TDMA_SCHED_DEBUG
    printf("Sending to parent, Type %d\r\n", tree_tx_buf[PKT_TYPE]);
#endif

    tree_rfTxInfo.pPayload = tree_tx_buf;
    //tree_rfTxInfo.length = TREE_TX_BUF_SIZE;
    tree_rfTxInfo.length = info_buf_size;
    
    // I thought this might be a good idea to try at first,
    // but the hardware recognition seems to be kind of funky
    // if not within a (very close) distance.  Straaaange yes?
    //tree_rfTxInfo.destAddr = my_parent;

    
    // Send out packet multiple times to notify.
    for (uint8_t i = 0; i < CSMA_SEND_REPEAT; i++)
    {

            b=(TREE_BACKOFF_TIME_MS*1000)/(((uint32_t)rand()%10)+1);
            nrk_spin_wait_us(b);

            while (!(v = _tdma_channel_check())) //channel busy
            {
#ifdef TDMA_SCHED_DEBUG
                nrk_kprintf(PSTR("Backoff\r\n"));
#endif
                //back off some set time, then check the channel again
                nrk_time_get(&cur_time);

                backoff_time.secs = TREE_BACKOFF_TIME_MS / 1000;
                backoff_time.nano_secs = (TREE_BACKOFF_TIME_MS % 1000) * NANOS_PER_MS;
                nrk_time_add(&backoff_time, backoff_time, cur_time);
                nrk_time_compact_nanos(&backoff_time);

                nrk_wait_until(backoff_time);
            }

            rf_rx_off();
            rf_tx_packet( &tree_rfTxInfo );
    }
}



void print_sensors_info()
{
    int8_t i;
    for (i = 0; i < DIM; i++)
    {
        if (sensorsInfo[i].isDead)
        {
            printf("Sensor %d is dead\r\n",i);
        }
        else
        {
            printf("Sensor %d:\r\n", i);
            printf("  address %d\r\n parent: %d\r\n  level: %d\r\n  children:  %d\r\n",
                sensorsInfo[i].address,
                sensorsInfo[i].parent,
                sensorsInfo[i].level,
                sensorsInfo[i].totalChildren);
            printf("  color: %d\r\n", sensorsInfo[i].color);
                //graphColors[sensorsInfo[i].color]);
                //graphColors[colors_used_level[ sensorsInfo[i].level  ]
                                              //[ sensorsInfo[i].color] ] );

        }

    }

}

#ifdef TDMA_MASTER_MODE
/* SCHEDULE PACKET FORMAT 
**
** No "stride" value exists currently.  Maybe later?
**
** Assuming max children = 5
** 
**  0     : TREE_SCHEDULE (control value)
**  1     : number of nodes in schedule
**  2     : number of this sched packet
**  3-4   : Addr of first node
**  5-6   : Slot with priority 0 (my slot)
**  7-8   : Stealable slot with priority 1
**  9-14  : Other stealable slots p1 to p_{max_children}
**  15-16 : TX slot to children (sync packet)
*/

void create_and_send_schedule()
{
    int8_t i,j,v;
    uint32_t b;
    uint8_t pkt_offset;
    uint8_t live_sensors;
    int8_t priority;
    uint8_t sched_pkt_num;
    uint8_t total_sched_pkts;
    uint8_t nodes_per_packet;
    // build the schedule and send it.  Probably a master-only thing.

    live_sensors = 0;

    for (i = 0; i < DIM; i++)
    {
        if (!sensorsInfo[i].isDead)
            live_sensors++;
    }

    tree_rfTxInfo.pPayload = tree_tx_buf;
    tree_rfTxInfo.pPayload[0] = TREE_SCHEDULE;
    //tree_rfTxInfo.pPayload[1] will have # of live nodes
    
#ifdef TDMA_SCHED_DEBUG
    printf("CASS: sensors %d\r\n", live_sensors);
#endif
    tree_rfTxInfo.pPayload[1] = live_sensors;

    // new stuff
    sched_pkt_num = 1;
    tree_rfTxInfo.pPayload[2] = sched_pkt_num;
    
    // size: control + # nodes + # packets + each node's size
    //total_sched_pkts = (live_sensors * (ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2)))
     //                    / (RF_MAX_PAYLOAD_SIZE-3);

    // how many nodes' schedule entries can we fit into a single packet
    nodes_per_packet = (TREE_BUF_SIZE-3) / 
                        (ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2));

    total_sched_pkts = live_sensors / nodes_per_packet;

    if ((live_sensors % nodes_per_packet) > 0)
        total_sched_pkts++;

//    if (((live_sensors * (ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2)))
//                 % (RF_MAX_PAYLOAD_SIZE-3))  > 0)
//            total_sched_pkts++;

#ifdef TDMA_SCHED_DEBUG
    printf("CASS: pkts %d\r\n", total_sched_pkts);
#endif
    pkt_offset = 3;

    for (i = 0; i < DIM; i++)
    {
        if (!sensorsInfo[i].isDead)
        {
            // deteremine if we can fit the next node in the schedule
            if (pkt_offset + ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2)  >= TREE_BUF_SIZE)
            {
                // We need a new packet
                tree_rfTxInfo.length = pkt_offset;

                for (uint8_t r = 0; r < CSMA_SEND_REPEAT; r++)
                {
                    b=(TREE_BACKOFF_TIME_MS*1000)/(((uint32_t)rand()%100)+1);
                    nrk_spin_wait_us(b);

                    while (!(v = _tdma_channel_check()))
                    {
                        rf_rx_off();
                        nrk_time_get(&cur_time);

                        backoff_time.secs = TREE_BACKOFF_TIME_MS / 1000;
                        backoff_time.nano_secs = (TREE_BACKOFF_TIME_MS % 1000) * NANOS_PER_MS;
                        nrk_time_add(&backoff_time, backoff_time, cur_time);
                        nrk_time_compact_nanos(&backoff_time);

                        nrk_wait_until(backoff_time);
                    }

                    rf_rx_off();
                    rf_tx_packet( &tree_rfTxInfo );               
                }

#ifdef TDMA_SCHED_DEBUG
                printf("Sent pkt %d/%d\r\n", sched_pkt_num, total_sched_pkts);
#endif

                nrk_spin_wait_us(5000);
                
                // first two bytes remain the same: Replace the third and move on.
                tree_rfTxInfo.pPayload[2] = ++sched_pkt_num;
                pkt_offset = 3;
            }
            
            //insert its color into p0
            
#ifdef TDMA_SCHED_DEBUG
            printf(" Slot %d for %d, offset %d\r\n", 
                get_slot_from_color(sensorsInfo[i].color,TDMA_TX_PARENT), i,pkt_offset);
#endif

            // put in the address
            tree_rfTxInfo.pPayload[pkt_offset] = (sensorsInfo[i].address >> 8);
            tree_rfTxInfo.pPayload[pkt_offset+1] = (sensorsInfo[i].address & 0xFF);

            pkt_offset+=2;

            // put in their priority 0 slot
            tree_rfTxInfo.pPayload[pkt_offset] =
                (get_slot_from_color(sensorsInfo[i].color,TDMA_TX_PARENT) >> 8);
            tree_rfTxInfo.pPayload[pkt_offset+1] = 
                (get_slot_from_color(sensorsInfo[i].color,TDMA_TX_PARENT) & 0xFF);

            priority = 1;
            // This might not be the right order to set priorities.  Look into this later.
            /*
            for (j = 0; j < DIM && priority < TREE_MAX_CHILDREN; j++)
            {
                if (i != j && (!sensorsInfo[j].isDead) && sensorsInfo[i].parent == sensorsInfo[j].parent)
                {
                    tree_rfTxInfo.pPayload[pkt_offset+(2*priority)] = 
                        (get_slot_from_color(sensorsInfo[j].color,TDMA_TX_PARENT) >> 8);
                    tree_rfTxInfo.pPayload[pkt_offset+(2*priority)+1] = 
                        (get_slot_from_color(sensorsInfo[j].color,TDMA_TX_PARENT) & 0xFF);
                    priority++;
                }
            }
            */
            // NEW PRIORITY METHOD.  Loop around list and add in order
            for (j = ((i+1) % DIM);
                 j!=i && priority < TREE_MAX_CHILDREN;
                 j=((j+1) % DIM))
            {
                if ((!sensorsInfo[j].isDead) && sensorsInfo[i].parent == sensorsInfo[j].parent)
                {
                    tree_rfTxInfo.pPayload[pkt_offset+(2*priority)] = 
                        (get_slot_from_color(sensorsInfo[j].color,TDMA_TX_PARENT) >> 8);
                    tree_rfTxInfo.pPayload[pkt_offset+(2*priority)+1] = 
                        (get_slot_from_color(sensorsInfo[j].color,TDMA_TX_PARENT) & 0xFF);
                    priority++;
                }
            }     

#ifdef TDMA_SCHED_DEBUG
            printf("Priority %d\r\n", priority);
#endif
            // fill other "Stealable" slot entries with -1 to say they are not so
            for (j = priority; j < TREE_MAX_CHILDREN; j++)
            {
                //printf("Encoded into %d\r\n", pkt_offset+(2*j));
                tree_rfTxInfo.pPayload[pkt_offset+(2*j)] = (((int16_t) -1) >> 8);
                tree_rfTxInfo.pPayload[pkt_offset+(2*j)+1] = (((int16_t) -1) & 0xFF);
            }
            
            pkt_offset += TREE_MAX_CHILDREN * 2;

            // Put in their tx slot to children, last
            tree_rfTxInfo.pPayload[pkt_offset] = 
                 (get_slot_from_color(sensorsInfo[i].color, TDMA_TX_CHILD) >> 8);
            tree_rfTxInfo.pPayload[pkt_offset+1] = 
                 (get_slot_from_color(sensorsInfo[i].color, TDMA_TX_CHILD) & 0xFF);

            pkt_offset += 2;

#ifdef TDMA_SCHED_DEBUG
            printf("pkt size %d\r\n", pkt_offset);
#endif
        }
    }

    if (pkt_offset > 3)
    {    
        //send last schedule packet
        tree_rfTxInfo.length = pkt_offset;

        for (uint8_t r = 0; r < CSMA_SEND_REPEAT; r++)
        {
            b=(TREE_BACKOFF_TIME_MS*1000)/(((uint32_t)rand()%100)+1);
            nrk_spin_wait_us(b);
#ifdef TDMA_SCHED_DEBUG
            printf("Last pkt wait %20"PRIu32"\r\n", b);
#endif

            while (!(v = _tdma_channel_check()))
            {
                rf_rx_off();
                nrk_time_get(&cur_time);

                backoff_time.secs = TREE_BACKOFF_TIME_MS / 1000;
                backoff_time.nano_secs = (TREE_BACKOFF_TIME_MS % 1000) * NANOS_PER_MS;
                nrk_time_add(&backoff_time, backoff_time, cur_time);
                nrk_time_compact_nanos(&backoff_time);

                nrk_wait_until(backoff_time);
            }

            rf_rx_off();
            rf_tx_packet( &tree_rfTxInfo );               
        }
#ifdef TDMA_SCHED_DEBUG
        printf("Sent pkt %d/%d\r\n", sched_pkt_num, total_sched_pkts);
#endif
    }

    //nrk_spin_wait_us(50000);
    
    // first two bytes remain the same: Replace the third and move on.
    //tree_rfTxInfo.pPayload[2] = sched_pkt_num++;
    //pkt_offset = 3;


    /* Pruint8_t packet
    nrk_kprintf(PSTR("PACKET: "));
    for (uint8_t offset = 0; offset < pkt_offset; offset++)
    {
        printf("%x ", tree_rfTxInfo.pPayload[offset]);
    }
    nrk_kprintf(PSTR("\r\n"));
    */

} // END CASS

// Master's function to obtain its schedule
void parse_schedule_master()
{
    uint8_t i, child_index;
    for (i = 0; i < DIM; i++)
    {
        if (!sensorsInfo[i].isDead)
        {
            if (sensorsInfo[i].address  == tree_addr16)
            {
                // if me, add my slots
                //tdma_schedule_add(get_slot_from_color(sensorsInfo[i].color, TDMA_TX_PARENT), TDMA_TX, 0);
                tdma_schedule_add(get_slot_from_color(sensorsInfo[i].color, TDMA_TX_CHILD) , TDMA_TX_CHILD, 0);
            }
            else
            {
                for (child_index = 0; child_index < num_children; child_index++)
                {
                    if (sensorsInfo[i].address == child_addrs[child_index])
                    {
                        //is an immediate child;  add RX slots
                        tdma_schedule_add(get_slot_from_color(sensorsInfo[i].color, TDMA_TX_PARENT), TDMA_RX, -1);
                    }
                }
            }
        }
    }
}

// only the master should need to do this
void parse_children_info(uint8_t * info_arr, uint8_t * offset,
                         uint8_t level, node_addr_t parent)
{
    uint8_t i;
    uint8_t n_children, n_neighbors;
    node_addr_t child, neighbor;

    if (level > depth_of_tree)
        depth_of_tree = level;

#ifdef TDMA_SCHED_DEBUG
    printf("Level %d cio %d\r\n", level, *offset); 
#endif
    child = info_arr[*offset];
    child <<= 8;
    child |= info_arr[(*offset)+1];
    
    n_children = info_arr[(*offset)+2];

#ifdef TDMA_SCHED_DEBUG
    printf("Child %d, has %d children, parent %d\r\n", child, n_children,parent);
#endif

    //sensorsInfo[child].totalChildren = n_children;
    sensorsInfo[child].address = child;
    sensorsInfo[child].totalChildren = 0;
    sensorsInfo[child].isDead        = 0;
    sensorsInfo[child].level         = level;
    sensorsInfo[child].parent        = parent;
    
    sensorsInfo[ parent ].children[ sensorsInfo[parent].totalChildren++ ] = child;

    Neighbors[child][parent] = 1;
    Neighbors[parent][child] = 1;

    (*offset) += 3;
#ifdef TDMA_SCHED_DEBUG
    printf("cio after add 3: %d\r\n",  *offset);
#endif
    
    for (i = 0; i < n_children; i++)
    {
        parse_children_info(info_arr, offset, level+1, child);
    }
    
    n_neighbors = info_arr[*offset];
    (*offset)++;
    
    for (i = 0; i < n_neighbors; i++)
    {
        neighbor = info_arr[*offset];
        neighbor <<= 8;
        neighbor |= info_arr[(*offset)+1];

#ifdef TDMA_SCHED_DEBUG
        printf("Child %d has neighbor %d\r\n", child, neighbor);
#endif
        (*offset)+=2;
        Neighbors[child][neighbor] = 1;
        Neighbors[neighbor][child] = 1;
    }

}


int8_t tdma_schedule_tree_make(uint8_t chan)
{
    int8_t ret;
    if (tdma_node_mode == TDMA_MASTER)
    {
        tdma_tree_init(chan);
        ret = tdma_tree_make_master();
    }
    else if (tdma_node_mode == TDMA_SLAVE)
    {
        tdma_tree_init(chan);
        ret = tdma_tree_make_slave();
    }
    else
        ret = NRK_ERROR;

    
    tdma_init_done = 0;
    return ret;
}

// Master code
// I send out to everyone without waiting

int8_t tdma_tree_make_master()
{
#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("in MTM\r\n"));
#endif

    if (tree_init_done == 0)
        return NRK_ERROR;


    my_level = 0;
#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("Will build tree\r\n"));
#endif
    send_tree_message();
    wait_for_responses();
    
    child_info_offset = 0;
    
    sensorsInfo[tree_addr16].address = tree_addr16;
    sensorsInfo[tree_addr16].level = my_level;
    sensorsInfo[tree_addr16].isDead = 0;
    sensorsInfo[tree_addr16].parent = 0;
    //sensorsInfo[tree_addr16].totalChildren = num_children;
    sensorsInfo[tree_addr16].totalChildren = 0;

#ifdef TDMA_SCHED_DEBUG
    printf( "I have %d children, cio is %d\r\n", num_children, child_info_offset);
#endif
    for (uint8_t i = 0; i < num_children; i++)
        parse_children_info(child_info, &child_info_offset, my_level+1, tree_addr16);
    
#ifdef TDMA_SCHED_DEBUG
    printf("mismatch: cio: %d, cis: %d\r\n", 
           child_info_offset, child_info_size);
#endif
    //color_graph();
    discrete_color();

#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("-----------SENSORS INFO--------------\r\n\r\n"));
    print_sensors_info();
    nrk_kprintf(PSTR("-----------END SENSORS INFO----------\r\n\r\n"));
#endif

    tree_rfTxInfo.pPayload = tree_tx_buf;
    create_and_send_schedule();
    parse_schedule_master();
#ifdef TDMA_SCHED_DEBUG
    tdma_schedule_print();
#endif

    tree_made=1;
    return NRK_OK;

}
#endif // TDMA_MASTER



// Slave code
//
// wait for a packet, then call the packet's owner my parent
#ifdef TDMA_SLAVE_MODE

int8_t tdma_tree_make_slave()
{
    uint16_t n;
    uint8_t message_type;
    uint8_t part_of_tree = 0;
    
    if (tree_init_done == 0)
        return NRK_ERROR;

#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("Beginning slave make tree operation\r\n"));
#endif
    nrk_led_set(ORANGE_LED);

    tree_rfRxInfo.max_length = TREE_BUF_SIZE;
    tree_rfRxInfo.pPayload = tree_rx_buf;
    rf_set_rx( &tree_rfRxInfo ,tree_channel);


    //while (rf_rx_check_fifop() == 0)
    //    ;
    while (!part_of_tree)
    {
        rf_polling_rx_on();

        while ((n=rf_polling_rx_packet()) == 0)
            ;

        //make sure a valid packet 
        if (n != 1)
        {
#ifdef TDMA_SCHED_DEBUG
            nrk_kprintf(PSTR("MKTREE_SLV: Invalid recv\r\n"));
#endif
            rf_rx_off();
            continue;
        }

        rf_rx_off();

        // packet is here, check for right type
        message_type = tree_rfRxInfo.pPayload[0];

#ifdef TDMA_SCHED_DEBUG
        printf("mkslvtree: Msg type %d\r\n", message_type);
#endif
        if (message_type != TREE_MAKE)
        {
#ifdef TDMA_SCHED_DEBUG
            nrk_kprintf(PSTR("MKTREE_SLV: msg type wrong. \r\n"));
#endif
            continue;
        }

        // Added for debugging
#ifdef DEBUG_RSSI
        if (tree_rfRxInfo.rssi < 0)
        {
            printf("RSSI low %d\r\n", tree_rfRxInfo.rssi);
            continue;
        }
#endif


        my_parent = tree_rfRxInfo.srcAddr;
        my_level = tree_rfRxInfo.pPayload[1]+1;
        
#ifdef TDMA_SCHED_DEBUG
        printf("My parent/level: %d/%d\r\n", my_parent, my_level);
#endif

        nrk_led_clr(ORANGE_LED);

        send_tree_message();
        wait_for_responses();
        notify_parent();
        
        obtain_schedule();

        part_of_tree = 1;
    }

#ifdef TDMA_SCHED_DEBUG
    tdma_schedule_print();
#endif

    tree_made=1;
    return NRK_OK;
}


// Slave's function to obtain schedule
// This includes waiting for the packet, parsing it
// and adding slots
void obtain_schedule()
{
    int8_t n;
    uint8_t v;
    uint32_t b;
    uint8_t i, j, pkt_offset, child_index;
    uint8_t message_type;
    uint8_t num_nodes;
    node_addr_t current_node_addr;
    tdma_slot_t new_slot;
    uint8_t have_schedule = 0;
    uint8_t current_pkt   = 0;
    uint8_t pkts_to_expect = 0;
    uint8_t pkts_received = 1;
    uint8_t nodes_per_packet;

    tree_rfRxInfo.max_length = TREE_BUF_SIZE;
    tree_rfRxInfo.pPayload = tree_rx_buf; 

    // Wait for a packet containing the schedule.
    
#ifdef TDMA_SCHED_DEBUG
    nrk_kprintf(PSTR("Wait for schedule\r\n"));
#endif

    while (!have_schedule)
    {
    
        tree_rfRxInfo.pPayload[0] = 0;
        message_type = 0;
        rf_polling_rx_on();
            
        while ((n=rf_polling_rx_packet()) == 0)
            ; 

        rf_rx_off();

        if (n != 1)
        {
#ifdef TDMA_SCHED_DEBUG
            nrk_kprintf(PSTR("SCHED: Invalid recv\r\n"));
#endif
            continue;
        }


        message_type = tree_rfRxInfo.pPayload[0];

        if (message_type != TREE_SCHEDULE)
        {
#ifdef TDMA_SCHED_DEBUG
            nrk_kprintf(PSTR("SCHED: Wrong msg type\r\n"));
#endif
            continue;
        }

        // We have a valid tree packet, begin parsing
        num_nodes = tree_rfRxInfo.pPayload[1];
        current_pkt = tree_rfRxInfo.pPayload[2];

        nodes_per_packet = (TREE_BUF_SIZE-3) / 
                         (ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2));

        pkts_to_expect = num_nodes / nodes_per_packet;

        if ((num_nodes % nodes_per_packet) > 0)
            pkts_to_expect++;

        // if this is first packet, we can find how many to expect
        /*
        pkts_to_expect = (num_nodes * (ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2)))
                         / (RF_MAX_PAYLOAD_SIZE-3);
        if (((num_nodes * (ADDR_SIZE + ((TREE_MAX_CHILDREN+1) * 2)))
                         % (RF_MAX_PAYLOAD_SIZE-3))  > 0)
            pkts_to_expect++;
        */


        if (current_pkt != pkts_received)
        {
#ifdef TDMA_SCHED_DEBUG
            printf("Recv %d need %d\r\n", 
                current_pkt, pkts_received);
#endif
            //if (current_pkt > pkts_received)
            //    nrk_halt();
            continue;
        }

        pkts_received++;

#ifdef TDMA_SCHED_DEBUG
        printf("Got pkt %d/%d. I'm %d\r\n", current_pkt,pkts_to_expect, tree_addr16);
#endif


        if (current_pkt == pkts_to_expect)
        {
#ifdef TDMA_SCHED_DEBUG
            printf("Have all pkts %d lv %d\r\n", tree_addr16, my_level);
#endif
            have_schedule = 1;
        }

    ///PRINT WHOLE PACKET
    /*
    nrk_kprintf(PSTR("PACKET: "));
    for (uint8_t offset = 0; offset < (tree_rfRxInfo.pPayload[1] * ((DIM+1) * ADDR_SIZE)) + 2; offset++)
    {
        printf("%x ", tree_rfRxInfo.pPayload[offset]);
    }
    nrk_kprintf(PSTR("\r\n"));
    */
    ///ENDPRINT

        //parse schedule
       

#ifdef TDMA_SCHED_DEBUG
        printf("Num Nodes: %d\r\n", num_nodes);
#endif

        pkt_offset = 3; //

        for (i = 0;
             i < num_nodes && 
                ((pkt_offset + (TREE_MAX_CHILDREN+1) * 2) < TREE_BUF_SIZE &&
                 (pkt_offset + (TREE_MAX_CHILDREN+1) * 2) < tree_rfRxInfo.length
                 )
              ; i++)
        {
            current_node_addr = tree_rfRxInfo.pPayload[pkt_offset]; 
            current_node_addr <<= 8;
            current_node_addr |= tree_rfRxInfo.pPayload[pkt_offset+1]; 

#ifdef TDMA_SCHED_DEBUG
            printf("OS: Got node %d. I'm %d offset %d\r\n",
                   current_node_addr, tree_addr16, pkt_offset);
#endif

            pkt_offset += 2;

            if (current_node_addr == tree_addr16)
            {
#ifdef TDMA_SCHED_DEBUG
                nrk_kprintf(PSTR("It's for me\r\n"));
#endif
                for (j = 0; j < TREE_MAX_CHILDREN; j++)
                {
                    new_slot = tree_rfRxInfo.pPayload[pkt_offset];
                    new_slot <<= 8;
                    new_slot |= tree_rfRxInfo.pPayload[pkt_offset+1];

#ifdef TDMA_SCHED_DEBUG
                    printf("Slot %d at offset %d\r\n", new_slot, pkt_offset);
#endif
                    if (new_slot > TDMA_SLOTS_PER_CYCLE)
                    {
#ifdef TDMA_SCHED_DEBUG
                        printf("IGNORING SLOT: %d\r\n", new_slot);
#endif
                    }
                    // If SLOT_STEALING is enabled, add all priorities.
                    // otherwise, just add the 0 priority slots (my TX slots)
#ifdef SLOT_STEALING
                    else if (new_slot >= 0)
                        tdma_schedule_add(new_slot, TDMA_TX_PARENT, j);
#else
                    else if (new_slot >= 0 && j == 0)
                        tdma_schedule_add(new_slot, TDMA_TX_PARENT, j);
#endif

                    pkt_offset+=2;
                }

                // add my TX_CHILD slot.  Priority can be 0 because slot
                // stealing among time packets is pretty impractical.
                new_slot = tree_rfRxInfo.pPayload[pkt_offset];
                new_slot <<= 8;
                new_slot |= tree_rfRxInfo.pPayload[pkt_offset+1];
            
                if (new_slot >= 0)
                    tdma_schedule_add(new_slot, TDMA_TX_CHILD, 0);

                pkt_offset+=2;

            }
            else if (current_node_addr == my_parent)
            {
                // add the parent's TX to child slot to my RX slots

                // seek past the priority slots
                pkt_offset += ((TREE_MAX_CHILDREN) * ADDR_SIZE);

                new_slot = tree_rfRxInfo.pPayload[pkt_offset];
                new_slot <<= 8;
                new_slot |= tree_rfRxInfo.pPayload[pkt_offset+1];

                if (new_slot >= 0)
                    tdma_schedule_add(new_slot, TDMA_RX, 1); //p1 to distinguish

                pkt_offset+=2;
            }
            else  // wasn't my address or parent's
            {
                // check if any child in my child array matches this node address
                for (child_index = 0; child_index < num_children; child_index++)
                {
                    if (child_addrs[child_index] == current_node_addr) // this refers to my child
                    {
                        new_slot = tree_rfRxInfo.pPayload[pkt_offset];
                        new_slot <<= 8;
                        new_slot |= tree_rfRxInfo.pPayload[pkt_offset+1];
                        // add this as a RX
#ifdef TDMA_SCHED_DEBUG
                        printf("Slot %d\r\n", new_slot);
#endif
                        if (new_slot >= 0)
                            tdma_schedule_add(new_slot, TDMA_RX, -1);
                        
                        break;
                    }
                }
                // skip past priority slots and TX_CHILD slot
                pkt_offset+= ((TREE_MAX_CHILDREN) * ADDR_SIZE) + 2;
            }
        }
        // end of this packet. Time to send.
#ifdef TDMA_SCHED_DEBUG
        printf("Me %d send offset %d\r\n", tree_addr16, pkt_offset);
#endif

        memcpy(tree_tx_buf, tree_rfRxInfo.pPayload, pkt_offset);
        tree_rfTxInfo.pPayload = tree_tx_buf;

        //printf("Type %d\r\n", tree_rfTxInfo.pPayload[0]);
        // length is predefined based on number of sensors
        tree_rfTxInfo.length = pkt_offset;// (tree_rfRxInfo.pPayload[1] * DIM) + 2;

        // Send out packet multiple times to notify.
        for (uint8_t i = 0; i < CSMA_SEND_REPEAT; i++)
        {

            b=((uint32_t)TREE_BACKOFF_TIME_MS*1000)/(((uint32_t)rand()%10)+1);
            nrk_spin_wait_us(b);

            while (!(v = _tdma_channel_check())) //channel busy
            {
#ifdef TDMA_SCHED_DEBUG
                nrk_kprintf(PSTR("SCHED: Backoff\r\n"));
#endif
                //back off some set time, then check the channel again
                nrk_time_get(&cur_time);

                backoff_time.secs = TREE_BACKOFF_TIME_MS / 1000;
                backoff_time.nano_secs = (TREE_BACKOFF_TIME_MS % 1000) * NANOS_PER_MS;
                nrk_time_add(&backoff_time, backoff_time, cur_time);
                nrk_time_compact_nanos(&backoff_time);

                nrk_wait_until(backoff_time);
            }

            rf_rx_off();
            rf_tx_packet( &tree_rfTxInfo );
        }

    }
}
#endif // TDMA_SLAVE

#ifdef TDMA_MASTER_MODE

// colors each node differently
void discrete_color()
{
    uint8_t i,j;

    j = 0;
    for ( i = 0; i < DIM; i++)
    {
        if (!sensorsInfo[i].isDead)
        {
            sensorsInfo[i].color = j++;
        }
    }
}
/*
 * Graph coloring algorithm as used in TDMA-ASAP simulation
 */

void color_graph()
{
	uint8_t count = 0;
	uint8_t nodesAlive = 0;
	uint8_t color = 0;
	uint8_t xyid;
    uint8_t x;
	int8_t current_level;	
	//uint8_t test = 0;
	//GCMCounter = 0;
	//GCMCounterSteal_C = 0;

	/*strcpy(graphColors[0], "red");
	strcpy(graphColors[1], "blue");
	strcpy(graphColors[2], "green");
	strcpy(graphColors[3], "yellow");
	strcpy(graphColors[4], "gray");
	strcpy(graphColors[5], "orange");
	strcpy(graphColors[6], "purple");
	strcpy(graphColors[7], "brown");
	strcpy(graphColors[8], "pink");
	strcpy(graphColors[9], "navy");
	strcpy(graphColors[10], "darkgreen");
	strcpy(graphColors[11], "gold");
	strcpy(graphColors[12], "lightgray");
	strcpy(graphColors[13], "darkorange");
	strcpy(graphColors[14], "magenta");
	strcpy(graphColors[15], "beige");
	strcpy(graphColors[16], "crimson");
	strcpy(graphColors[17], "skyblue");
	strcpy(graphColors[18], "forestgreen");
	strcpy(graphColors[19], "lightyellow");
	strcpy(graphColors[20], "slategray");
	strcpy(graphColors[21], "orchid");
	strcpy(graphColors[22], "peru");
	strcpy(graphColors[23], "tomato");
	strcpy(graphColors[24], "midnightblue");
	strcpy(graphColors[25], "limegreen");
	strcpy(graphColors[26], "yellowgreen");
	strcpy(graphColors[27], "violet");
	strcpy(graphColors[28], "tan");
	strcpy(graphColors[29], "maroon");
    */
	
for(uint8_t j = 0; j < DIM; j++)
{
	if (sensorsInfo[j].isDead)
	{
#ifdef TDMA_SCHED_DEBUG
		printf("Node: %d is Dead\r\n", j);
#endif
	}
}



////Test - Pruint8_t Tree
//for(uint8_t i = 0; i <= depth_of_tree[1]; i++)
//{
//	printf("Level: %d, ", i);
//	for(uint8_t j = 0; j < DIM; j++)
//	{
//		if (i == sensorsInfo[j].level)
//			printf("Node: %d , parent: %d ", sensorsInfo[j].xy, sensorsInfo[j].parent);
//	}
//	printf("\n");
//}
	
	//Fill uncolored_node_list
	for(int8_t j = depth_of_tree; j >= 0 ; j--)
	{
		for(int8_t i = 0; i < DIM; i++)	
		{
			if(sensorsInfo[i].level == j && !sensorsInfo[i].isDead)
			{
				uncolored_node_list[0][count] = i;
				uncolored_node_list[1][count] = j;
				count++;	
			}
		}
	}
nodesAlive = count;
	
//printf("\nVerify uncolored node list\n");
//printf("Count: %d\n", count);
//for(uint8_t i = 0; i < count; i++)
//{
//	printf("uncolored node list: %d %d\n", uncolored_node_list[0][i], uncolored_node_list[1][i]);	
//}


	
//	printf("Verifying sort\n");		
//	for (uint8_t i = 0; i < DIM; i++)
//	{
//		if (i > 0)
//		{
//			if(uncolored_node_list[1][i] > uncolored_node_list[1][i -1])
//			{
//				printf("\n\nWE HAVE A PROBLEM\n\n");
//			}
//		}
//	}
//	printf("Nodes sorted okay\n");
	
	

	//Determine how many slots we need for this tree				
	//Initialize each sensor's color and the ableToSteal flags
	for(uint8_t i = 0; i < DIM; i++)
	{
		sensorsInfo[i].color = -1;

//Robert Cleric commented out 5/10/07		
//		if (sensorsInfo[i].level != -1)
//		{
/*
			if (steal_C)
			{
				for (uint8_t j = 0; j < MAX_SIBLINGS; j++)
				{
					sensorsInfo[i].ableToSteal[j] = -1;
					sensorsInfo[i].ableToSteal[j] = -1;
				}
			}
*/
//		}
	}
//**printf("\nRegular coloring, used for steal_C, steal_G, ZMAC\n");
	//Pruint8_t Number of nodes per level to get an idea of structure
//**	printf("Number of nodes on each level\n");
//**	for(uint8_t j = 0; j <= depth_of_tree[1]; j++)
//**	{
//**		for(uint8_t i = 0; i < DIM; i++)
//**		{
//**			if (j == sensorsInfo[i].level)
//**				test++;
//**		}
//**		printf("Level:, %d, # Nodes:, %d\n" , j, test);
//**		test = 0;
//**	}
//**	printf("\n");

	//Start at the bottom and work your way up
	for(int8_t j = depth_of_tree; j >= 0 ; j--)
	{
//printf("J: %d  nodesAlive:%d\n", j, nodesAlive);
		uint8_t first_node_for_level = 1;
		uint8_t more_to_color_on_this_level = 1;
		color = 0;	//This is the first color
		count = 0;
		while(more_to_color_on_this_level)
		{	
			current_level = -1;
			for(uint8_t i = 0; i < nodesAlive; i++)
			{
				//if(sensorsInfo[i].level != -1)
				//{
//	printf("uncolored_node_list[1][%d]: %d\n", i, uncolored_node_list[1][i]);
					if (uncolored_node_list[1][i] == j)
					{
	
//	printf("Level: %d Node: %d made it in loop\n", j, i);
						current_level = uncolored_node_list[1][i];
						uncolored_node_list[1][i] = -1;
						xyid = uncolored_node_list[0][i];
					
						//This node is on the current level
						if(first_node_for_level)
						{
							//This is the first node so just assign it the first color
                            
                            // JOHN:  ^^^ WHAAAAAT?  That's not a graph coloring algorithm!!!
                            // Begin JCY

                            do
                            {
                                    child_has_color = 0;
                                    for (x = 0; x < sensorsInfo[xyid].totalChildren; x++)
                                    {
#ifdef TDMA_SCHED_DEBUG
                                        printf("Testing for matches: %d\r\n", x);
                                        printf("color: %d, child %d \r\n", 

                                        color, sensorsInfo[ sensorsInfo[xyid].children[x] ].color);
#endif
                                        if (color == sensorsInfo[ sensorsInfo[xyid].children[x] ].color)
                                        {
#ifdef TDMA_SCHED_DEBUG
                                            nrk_kprintf(PSTR("Color matches\r\n"));
#endif
                                            child_has_color = 1;
                                        }
                                    }
                                    if(child_has_color)
                                        color++;
                            } while (child_has_color);
                            //if (sensorsInfo[xyid].parent != 0 && 
                            //    sensorsInfo[ sensorsInfo[xyid].parent ].color == color)


                            // End JCY
#ifdef TDMA_SCHED_DEBUG
                            printf("SETTING COLOR OF NODE %d to %d\r\n", xyid, color);
#endif
							sensorsInfo[xyid].color = color;
							//Add this node id to the colored node list
							colored_node_list[count] = xyid;
							count++;
							first_node_for_level = 0;
						
							//GCMCounter++;
						}
						else
						{
							//This is not the first node 
							//We need to determine which other nodes can go 
							//into this color
							//Loop through each node that is colored to see if
							//this node can be the same color
							uint8_t okay_to_add_node = 1;
							for(uint8_t k = 0; k < count; k++)
							{
								//Robert Cleric modified 8/6/06
								if(sensorsInfo[xyid].parent == sensorsInfo[colored_node_list[k]].parent
                                  || Neighbors[xyid][sensorsInfo[colored_node_list[k]].parent] 
                                  || Neighbors[sensorsInfo[xyid].parent][colored_node_list[k]])	
								{
									//It is not okay to add this node
									okay_to_add_node = 0;																				
								}
								//GCMCounter++;
							}

                            // BEGIN JCY
                            // Also, make sure none of children are using the same color, dammit!
                            for (x = 0; x < sensorsInfo[xyid].totalChildren; x++)
                            {
#ifdef TDMA_SCHED_DEBUG
                                printf("Testing colors %d %d\r\n", color,
                                    sensorsInfo[ sensorsInfo[xyid].children[x] ].color);
#endif
                                if (color == sensorsInfo[ sensorsInfo[xyid].children[x] ].color)
                                {
                                    okay_to_add_node = 0;
                                }

                            }
                            // and parent ...
                            if (color == sensorsInfo[ sensorsInfo[xyid].parent ].color)
                            {
                                okay_to_add_node = 0;
                            }

                            // END JCY
							if (okay_to_add_node)
							{
								//It is okay to color this node the current color
#ifdef TDMA_SCHED_DEBUG
            printf("SETTING COLOR OF NODE %d to %d\r\n", xyid, color);
#endif
								sensorsInfo[xyid].color = color;
								//Add this node id to the colored node list
								colored_node_list[count] = xyid;
								count++;
							
								//Now that the node has been colored, check to see if it is able to steal any slots
								/*if (steal_C)
								{
									//Make sure that this is not the first color
									//If it is, I am the in the first slot and have nothing to steal 
									if(sensorsInfo[sensorsInfo[xyid].parent].totalChildren > 1)
									{
										for(uint8_t m = color - 1; m >= 0; m--)
										{
											//Check to see that if my sibling did not exist
											//would I then be able to send?
										
											bool okayToProceed = false;
											//First I need to know if I have a sibling that is color m
											for(uint8_t p = 0; p < sensorsInfo[sensorsInfo[xyid].parent].totalChildren; p++)
											{
												if(sensorsInfo[sensorsInfo[sensorsInfo[xyid].parent].children[p]].color == m)
												{
													//I also need to know if that sibling is within range
													if(Neighbors[xyid][sensorsInfo[sensorsInfo[xyid].parent].children[p]])
														okayToProceed = true;
												}
											}
										
											if(okayToProceed)
											{		
												//If I am going to try to steal slot i, I must first make sure
												//that I am able to steal slot i + 1 or I cannot gaurantee that 
												//I will be able to steal slot i.
												if(m == (color-1))
												{
													//This can be implemented much more efficiently
													//Loop through each node on this level
													//and check to see if the current node can be scheduled with m
													bool canSteal = true;
													for(uint8_t n = 0; n < DIM; n++)
													{
														if(!sensorsInfo[n].isDead && sensorsInfo[n].level == j && sensorsInfo[n].color == m && (sensorsInfo[n].parent != sensorsInfo[xyid].parent))
														{
															//This node is on the right level and the color we are looking for
															if(Neighbors[xyid][sensorsInfo[n].parent])	
															{
																//It is not okay to steal this slot
																canSteal = false;																				
															}				
														
														}
														//GCMCounterSteal_C++;
													}
													if(canSteal)
													{
														sensorsInfo[xyid].ableToSteal[m] = 1;  //I can steal
													}
													else
														sensorsInfo[xyid].ableToSteal[m] = 0;	//I can't steal
												}
												else if(sensorsInfo[xyid].ableToSteal[m+1] == 1) //This condition limits the nodes that can be stolen but I do not see a way around it
												{
													//This can be implemented much more efficiently
													//Loop through each node on this level
													//and check to see if the current node can be scheduled with m
													bool canSteal = true;
													for(uint8_t n = 0; n < DIM; n++)
													{
														if(!sensorsInfo[n].isDead && sensorsInfo[n].level == j && sensorsInfo[n].color == m && (sensorsInfo[n].parent != sensorsInfo[xyid].parent))
														{
															//This node is on the right level and the color we are looking for
															if(Neighbors[xyid][sensorsInfo[n].parent])	
															{
																//It is not okay to steal this slot
																canSteal = false;																				
															}				
															
														}
														//GCMCounterSteal_C++;
													}
													if(canSteal)
													{
														sensorsInfo[xyid].ableToSteal[m] = 1;
													}
													else
														sensorsInfo[xyid].ableToSteal[m] = 0;
												}
												else
												{
													//GCMCounterSteal_C++;
													break;
												}
											}
										}
									}
								}*/
							}
							else
							{
								//Since this node could not be colored, it must be added back
								//into the uncolored node list
								uncolored_node_list[1][i] = current_level;
							}
						}		
					}
					//else
						//GCMCounter++;
				//}		
			}
			color++; //Increment to next color to use
			count = 0; //reset count for new color
			if(current_level == -1)
			{
				more_to_color_on_this_level = 0;
				colors_used[j] = color -1;
				colors_used_level[j][color-2] = color -2;
			}
		}
	}
}
#ifdef COLOR_OPT
void color_graph2()
{
	uint8_t count = 0;
	uint8_t color = 0;
	uint8_t xyid;
	uint8_t current_level;	
    uint8_t x;
	
	uint8_t total_colors_used2 = 0;
	
////Test - Pruint8_t Tree
//for(uint8_t i = 0; i <= depth_of_tree[1]; i++)
//{
//	printf("Level: %d, ", i);
//	for(uint8_t j = 0; j < DIM; j++)
//	{
//		if (i == sensorsInfo[j].level)
//			printf("Node: %d , parent: %d ", sensorsInfo[j].xy, sensorsInfo[j].parent);
//	}
//	printf("\n");
//}
	
	//Fill uncolored_node_list
	for(uint8_t j = depth_of_tree; j >= 0 ; j--)
	{
		for(uint8_t i = 0; i < DIM; i++)	
		{
			if(sensorsInfo[i].level == j && !sensorsInfo[i].isDead)
			{
				uncolored_node_list[0][count] = i;
				uncolored_node_list[1][count] = j;
				count++;
			}
		}
	}

uint8_t nodesAlive = count;
//	printf("Verifying sort\n");		
//	for (uint8_t i = 0; i < DIM; i++)
//	{
//		if (i > 0)
//		{
//			if(uncolored_node_list[1][i] > uncolored_node_list[1][i -1])
//			{
//				printf("\n\nWE HAVE A PROBLEM\n\n");
//			}
//		}
//	}


	//Determine how many slots we need for this tree				
	//Initialize each sensor's color and ableToSteal flags
	for(uint8_t i = 0; i < DIM; i++)
	{
		sensorsInfo[i].color2 = -1;
		sensorsInfo[xyid].schedWithDiffLevel = -1;
//		if (steal_C)
//		{
//			for (uint8_t j = 0; j < MAX_SIBLINGS; j++)
//			{
//				sensorsInfo[i].ableToSteal[j] = false;
//			}
//		}
	}
			
//	//Pruint8_t Number of nodes per level to get an idea of structure
//	printf("\nNumber of nodes on each level\n");
//	for(uint8_t j = 0; j <= depth_of_tree[1]; j++)
//	{
//		for(uint8_t i = 0; i < DIM; i++)
//		{
//			if (j == sensorsInfo[i].level)
//				test++;
//		}
//		printf("Level: %d, # Nodes: %d\n" , j, test);
//		test = 0;
//	}
//	printf("\n");

	//Start at the bottom and work your way up
	for(uint8_t j = depth_of_tree; j >= 0 ; j--)
	{
		uint8_t first_node_for_level = 1;
		uint8_t more_to_color_on_this_level = 1;
		color = 0;	//This is the first color
		count = 0;
		while(more_to_color_on_this_level)
		{	
			current_level = -1;
			for(uint8_t i = 0; i < nodesAlive; i++)
			{	
				//if (sensorsInfo[i].level != -1)
				//{
					if (uncolored_node_list[1][i] == j)
					{
						current_level = uncolored_node_list[1][i];
						uncolored_node_list[1][i] = -1;
						xyid = uncolored_node_list[0][i];
					
						//This node is on the current level
						if(first_node_for_level)
						{
							//This is the first node so just assign it the first color
							sensorsInfo[xyid].color2 = color;
							//Add this node id to the colored node list
							colored_node_list2[count] = xyid;
							count++;
							first_node_for_level = 0;
						}
						else
						{
							//This is not the first node 
							//We need to determine which other nodes can go 
							//into this color
							//Loop through each node that is colored to see if
							//this node can be the same color
							uint8_t okay_to_add_node = 1;
							for(uint8_t k = 0; k < count; k++)
							{
								//Robert Cleric modified 8/6/06
								if(sensorsInfo[xyid].parent == sensorsInfo[colored_node_list2[k]].parent || Neighbors[xyid][sensorsInfo[colored_node_list2[k]].parent] || Neighbors[sensorsInfo[xyid].parent][colored_node_list2[k]] )	
								{
									//It is not okay to add this node
									okay_to_add_node = 0;																				
								}
							}
							if (okay_to_add_node)
							{
								//It is okay to color this node the current color
								sensorsInfo[xyid].color2 = color;
								//Add this node id to the colored node list
								colored_node_list2[count] = xyid;
								count++;								
							}
							else
							{
								//Since this node could not be colored, it must be added back
								//into the uncolored node list
								uncolored_node_list[1][i] = current_level;
							}
						}		
					}
				//}//new
			}
			color++; //Increment to next color to use
			count = 0; //reset count for new color
			if(current_level == -1)
			{
				more_to_color_on_this_level = 0;
				colors_used2[j] = color -1;				
				colors_used_level2[j][color-2] = color -2;
			}
		}
		
		//Optimization 2
		//Optimization 2 - after this level has been colored go up
		//some levels and see if there are any nodes with no children
		//that can also be scheduled with these nodes.	
			
		//Loop through each node on the levels i - 1, i - 2, i -3 
		uint8_t levelsToGoUp = 0;
		uint8_t count2 = 0;
		
		if (j == 0)
			levelsToGoUp = 0;
		else if (j==1)
			levelsToGoUp = 1;
		else if (j==2)
			levelsToGoUp = 2;
		else
			levelsToGoUp = 3;

		//for(uint8_t m = 0; m < levelsToGoUp; m++)
		for(uint8_t k = 0; k < (colors_used2[j]); k++)
		{
			current_level = -1;
			count2 = 0;
			//Make a list of colored nodes
			for(uint8_t p = 0; p < DIM; p++)
			{
				if(sensorsInfo[p].color2 == k && sensorsInfo[p].level == j && !sensorsInfo[p].isDead)
				{
					colored_node_list[count2] = p;
					count2++;				
				}
			}
			//for(uint8_t k = 0; k < (colors_used2[j]); k++)
			for(uint8_t m = 0; m < levelsToGoUp; m++)
			{				
				for(uint8_t i = 0; i < DIM; i++)
				{
					if (sensorsInfo[i].level != -1 && !sensorsInfo[i].isDead)
					{
						if (uncolored_node_list[1][i] == (j-(m+1)))
						{
							xyid = uncolored_node_list[0][i];
							//Check to see if this node has no children
							if(sensorsInfo[xyid].totalChildren == 0)
							{						
								current_level = uncolored_node_list[1][i];
								uncolored_node_list[1][i] = -1;						
								//Determine if other nodes can go 
								//into this color
								//Loop through each node on level i that is colored to see if
								//this node can be one of those colors
						
								uint8_t okay_to_add_node = 1;
								uint8_t tempNode;
								//Check each node
								for (uint8_t n = 0; n < count2; n++)
								{
									//See if the node is the proper color
									//and if it is on the proper level								
									//Robert Cleric modified 8/6/06
									//Check to see if the current node can be scheduled with the node that is already colored
									if(sensorsInfo[xyid].parent == sensorsInfo[colored_node_list[n]].parent || Neighbors[xyid][sensorsInfo[colored_node_list[n]].parent] || Neighbors[sensorsInfo[xyid].parent][colored_node_list[n]] )										
									{
										//It is not okay to add this node
										okay_to_add_node = 0;																				
									}	
									tempNode = colored_node_list[n];				
								}
								if (okay_to_add_node)
								{
									//It is okay to color this node the current color
									sensorsInfo[xyid].color2 = sensorsInfo[tempNode].color2;
									sensorsInfo[xyid].schedWithDiffLevel = j;
									colored_node_list[count2] = xyid;
									count2++;
								}
								if(!okay_to_add_node)
								{							
									//Since this node could not be colored, it must be added back
									//into the uncolored node list
									uncolored_node_list[1][i] = current_level;
								}
							}
						}
					}//New
				}
			}
		}
	}

}
//*****************************************************************************************
/* color_graph4, uses optimization */
//*****************************************************************************************

void color_graph4()
{
	uint8_t count = 0;
	uint8_t color = 0;
	uint8_t xyid;
	uint8_t current_level;	
	
	uint8_t total_colors_used4 = 0;
    uint8_t x;
	
	
	//Fill uncolored_node_list
	for(uint8_t j = depth_of_tree; j >= 0 ; j--)
	{
		for(uint8_t i = 0; i < DIM; i++)	
		{
			if(sensorsInfo[i].level == j && !sensorsInfo[i].isDead)
			{
				uncolored_node_list[0][count] = i;
				uncolored_node_list[1][count] = j;
				count++;
			}
		}
	}
uint8_t nodesAlive = count;
	//Determine how many slots we need for this tree				
	//Initialize each sensor's color and ableToSteal flags
	for(uint8_t i = 0; i < DIM; i++)
	{
		sensorsInfo[i].color4 = -1;
		sensorsInfo[xyid].schedWithDiffLevel4 = -1;
//		if (steal_C)
//		{
//			for (uint8_t j = 0; j < MAX_SIBLINGS; j++)
//			{
//				sensorsInfo[i].ableToSteal[j] = false;
//			}
//		}
	}
			

	//Start at the bottom and work your way up
	for(uint8_t j = depth_of_tree; j >= 0 ; j--)
	{
		uint8_t first_node_for_level = 1;
		uint8_t more_to_color_on_this_level = 1;
		color = 0;	//This is the first color
		count = 0;
		while(more_to_color_on_this_level)
		{	
			current_level = -1;
			for(uint8_t i = 0; i < nodesAlive; i++)
			{	
				//if(sensorsInfo[i].level != -1)
				//{
					if (uncolored_node_list[1][i] == j)
					{
						current_level = uncolored_node_list[1][i];
						uncolored_node_list[1][i] = -1;
						xyid = uncolored_node_list[0][i];
					
						//This node is on the current level
						if(first_node_for_level)
						{
							//This is the first node so just assign it the first color


                            // JOHN:  ^^^ WHAAAAAT?  That's not a graph coloring algorithm!!!
                            // Begin JCY

                            do
                            {
                                    child_has_color = 0;
                                    for (x = 0; x < sensorsInfo[xyid].totalChildren; x++)
                                    {
                                        printf("Testing for matches: %d\r\n", x);
                                        printf("color: %d, child %d \r\n", 
                                        color, sensorsInfo[ sensorsInfo[xyid].children[x] ].color);
                                        if (color == sensorsInfo[ sensorsInfo[xyid].children[x] ].color)
                                        {
                                            nrk_kprintf(PSTR("Color matches\r\n"));
                                            child_has_color = 1;
                                        }
                                    }
                                    if(child_has_color)
                                        color++;
                            } while (child_has_color);
                            //if (sensorsInfo[xyid].parent != 0 && 
                            //    sensorsInfo[ sensorsInfo[xyid].parent ].color == color)


                            // End JCY

							sensorsInfo[xyid].color4 = color;
							//Add this node id to the colored node list
							colored_node_list4[count] = xyid;
							count++;
							first_node_for_level = 0;
						}
						else
						{
							//This is not the first node 
							//We need to determine which other nodes can go 
							//into this color
							//Loop through each node that is colored to see if
							//this node can be the same color
							uint8_t okay_to_add_node = 1;
							for(uint8_t k = 0; k < count; k++)
							{	
								//Robert Cleric modified 8/6/06
								if(sensorsInfo[xyid].parent == sensorsInfo[colored_node_list4[k]].parent || 
                                        Neighbors[xyid][sensorsInfo[colored_node_list4[k]].parent] || 
                                        Neighbors[sensorsInfo[xyid].parent][colored_node_list4[k]] )	
								{
									//It is not okay to add this node
									okay_to_add_node = 0;																				
								}
							}


                            // BEGIN JCY
                            // Also, make sure none of children are using the same color, dammit!
                            for (x = 0; x < sensorsInfo[xyid].totalChildren; x++)
                            {
                                printf("Testing colors %d %d\r\n", color,
                                    sensorsInfo[ sensorsInfo[xyid].children[x] ].color);
                                if (color == sensorsInfo[ sensorsInfo[xyid].children[x] ].color)
                                {
                                    okay_to_add_node = 0;
                                }

                            }
                            // and parent ...
                            if (color == sensorsInfo[ sensorsInfo[xyid].parent ].color)
                            {
                                okay_to_add_node = 0;
                            }

                            // END JCY


                            
							if (okay_to_add_node)
							{
								//It is okay to color this node the current color
								sensorsInfo[xyid].color4 = color;
								//Add this node id to the colored node list
								colored_node_list4[count] = xyid;
								count++;								
							}
							else
							{
								//Since this node could not be colored, it must be added back
								//into the uncolored node list
								uncolored_node_list[1][i] = current_level;
							}
						}		
					}
					//else
				//}//NEW
			}
			color++; //Increment to next color to use
			count = 0; //reset count for new color
			if(current_level == -1)
			{
				more_to_color_on_this_level = 0;
				colors_used4[j] = color -1;				
				colors_used_level4[j][color-2] = color -2;
			}
		}
		
		//Optimization 4
		uint8_t count2 = 0;
		//Scheudle all leaf nodes possible with the lowest level
		if(j==depth_of_tree)
		{
			for(uint8_t k = 0; k < colors_used4[j]; k++)
			{
				current_level = -1;
				count2 = 0;
				//Make a list of colored nodes
				for(uint8_t p = 0; p < DIM; p++)
				{
					if(sensorsInfo[p].color2 == k && sensorsInfo[p].level == j && !sensorsInfo[p].isDead)
					{
						colored_node_list[count2] = p;
						count2++;				
					}
				}
				
				
				
				for(uint8_t i = 0; i < DIM; i++)
				{
					if (sensorsInfo[i].level != -1 && !sensorsInfo[i].isDead)
					{
						//if (uncolored_node_list[1][i] == (j-(m+1)))
						if((sensorsInfo[i].totalChildren == 0) && (uncolored_node_list[1][i] != -1))
						{
							xyid = uncolored_node_list[0][i];
							
							current_level = uncolored_node_list[1][i];
							uncolored_node_list[1][i] = -1;						
						
							//Determine if this node will fit in with this color
							//Loop through each node on level i that is colored to see if
							//this node can be one of those colors
						
							uint8_t okay_to_add_node = 1;
							uint8_t tempNode;
							//Check each node
							for (uint8_t n = 0; n < count2; n++)
							{
								//See if the node is the proper color
								//and if it is on the proper level								
								//Robert Cleric modified 8/6/06
								//Check to see if the current node can be scheduled with the node that is already colored
								if(sensorsInfo[xyid].parent == sensorsInfo[colored_node_list4[n]].parent || Neighbors[xyid][sensorsInfo[colored_node_list4[n]].parent] || Neighbors[sensorsInfo[xyid].parent][colored_node_list4[n]] )										
								{
									//It is not okay to add this node
									okay_to_add_node = 0;																				
								}	
								tempNode = colored_node_list[n];				
							}
							if (okay_to_add_node)
							{
								//It is okay to color this node the current color
								sensorsInfo[xyid].color4 = sensorsInfo[tempNode].color4;
								sensorsInfo[xyid].schedWithDiffLevel4 = j;
								colored_node_list4[count2] = xyid;
								count2++;
							}
							if(!okay_to_add_node)
							{							
								//Since this node could not be colored, it must be added back
								//into the uncolored node list
								uncolored_node_list[1][i] = current_level;
							}	
						}
//						else
//							GCMCounter10++;
					}//NEW
				}
			}
		
		
		
printf("\nChecking to see how many total leaf nodes were scheduled\n");
uint8_t leafCount = 0;
uint8_t coloredLeafCount = 0;
for (uint8_t i = 0; i < DIM; i++)
{
	if (sensorsInfo[i].level != -1 && !sensorsInfo[i].isDead)
	{
		if(sensorsInfo[i].totalChildren == 0)
		{
			leafCount++;
			if(sensorsInfo[i].color4 != -1)
				coloredLeafCount++;
		}
	}
	
}
printf("Total Leafs:, %d\n", leafCount);
printf("Colored Leafs:, %d\n", coloredLeafCount);
printf("\n");
		}
		
		
		
/*		//Optimization 2 - after this level has been colored go up
		//some levels and see if there are any nodes with no children
		//that can also be scheduled with these nodes.	
		
		//Loop through each node on the levels i - 1, i - 2, i -3 
		uint8_t levelsToGoUp = 0;
		uint8_t count2 = 0;
		
		if (j == 0)
			levelsToGoUp = 0;
		else if (j==1)
			levelsToGoUp = 1;
		else if (j==2)
			levelsToGoUp = 2;
		else
			levelsToGoUp = 3;

		//for(uint8_t m = 0; m < levelsToGoUp; m++)
		for(uint8_t k = 0; k < (colors_used2[j]); k++)
		{
			current_level = -1;
			count2 = 0;
			//Make a list of colored nodes
			for(uint8_t p = 0; p < DIM; p++)
			{
				if(sensorsInfo[p].color2 == k && sensorsInfo[p].level == j)
				{
					colored_node_list[count2] = p;
					count2++;				
				}
			}
			for(uint8_t m = 0; m < levelsToGoUp; m++)
			{				
				for(uint8_t i = 0; i < DIM; i++)
				{
					if (uncolored_node_list[1][i] == (j-(m+1)))
					{
						xyid = uncolored_node_list[0][i];
						//Check to see if this node has no children
						if(sensorsInfo[xyid].totalChildren == 0)
						{						
							current_level = uncolored_node_list[1][i];
							uncolored_node_list[1][i] = -1;						
							//Determine if other nodes can go 
							//into this color
							//Loop through each node on level i that is colored to see if
							//this node can be one of those colors
						
							uint8_t okay_to_add_node = 1;
							uint8_t tempNode;
							//Check each node
							for (uint8_t n = 0; n < count2; n++)
							{
								//See if the node is the proper color
								//and if it is on the proper level								
								//Robert Cleric modified 8/6/06
								//Check to see if the current node can be scheduled with the node that is already colored
								if(sensorsInfo[xyid].parent == sensorsInfo[colored_node_list[n]].parent || Neighbors[xyid][sensorsInfo[colored_node_list[n]].parent] || Neighbors[sensorsInfo[xyid].parent][colored_node_list[n]] )										
								{
									//It is not okay to add this node
									okay_to_add_node = 0;																				
								}	
								tempNode = colored_node_list[n];				
								GCMCounter3++;
							}
							if (okay_to_add_node)
							{
								//It is okay to color this node the current color
								sensorsInfo[xyid].color2 = sensorsInfo[tempNode].color2;
								sensorsInfo[xyid].schedWithDiffLevel = j;
								colored_node_list[count2] = xyid;
								count2++;
							}
							if(!okay_to_add_node)
							{							
								//Since this node could not be colored, it must be added back
								//into the uncolored node list
								uncolored_node_list[1][i] = current_level;
							}
						}
						else
							GCMCounter3++;
					}
					else
						GCMCounter3++;
				}
			}
		}*/
	}
		

//	printf("Done coloring graph OPT2\n");		

//**	//Pruint8_t out the number of colors used on each level
//**	printf("\nNumber of colors used on each level for OPT4\n");
//**	for( uint8_t i = 0; i <= depth_of_tree[1]; i++)
//**	{
//**		printf("Level:, %d, colors:, %d\n", i, colors_used4[i]);
//**		total_colors_used4 = total_colors_used4 + colors_used4[i];
//**	}
//**	printf("\nTotal colors/slots used:, %d\n", total_colors_used4);
		
//printf("Verifying coloring of OPT2\n");
//uint8_t found = 0;
//for(uint8_t i = 0; i < DIM; i++)
//{
//	found = 0;
//	for(uint8_t j = 0; j < colors_used[sensorsInfo[i].level]; j++)
//	{
//		if (sensorsInfo[i].color2 == j)
//			found = 1;
//	}
//	if(!found)
//		printf("\n\n\nWE HAVE A PROBLEM\n\n\n");
//}

//uint8_t test = 0;
//for (uint8_t i = 0; i < DIM; i++)
//{
//	if(sensorsInfo[i].color2 == -1)
//		test++;
//}
//
//printf("Uncolored Nodes: %d\n", test);
	
}

#endif
#endif
