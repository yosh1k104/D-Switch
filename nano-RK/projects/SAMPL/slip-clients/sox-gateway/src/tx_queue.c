#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <tx_queue.h>


void tx_q_init (void)
{
  int i;
  for (i = 0; i < MAX_Q_LEN; i++)
    tx_q[i].ready = 0;
  q_index = 0;
}

uint8_t tx_q_pending ()
{
  int i;
  for (i = 0; i < MAX_Q_LEN; i++)
    if (tx_q[i].ready == 1)
      return 1;
  return 0;
}

uint8_t tx_q_get (char *msg)
{
  int i, j, found, size;
  int max_prio, selected_index;

  selected_index=0;
  max_prio=0;
  found = 0;
  j = q_index;
  for (i = 0; i < MAX_Q_LEN; i++) {
    if (tx_q[j].ready == 1) {
      found = 1;
      if(tx_q[j].priority>max_prio)
	{
      		selected_index=j;
		max_prio=tx_q[j].priority;
	}
      break;
    }
    j++;
    if (j >= MAX_Q_LEN)
      j = 0;
  }
  
  if (found == 0)
    return 0;

  j=selected_index;
// copy pkt and return size
  size = tx_q[j].size;
  for (i = 0; i < size; i++)
    msg[i] = tx_q[j].pkt[i];
  tx_q[j].ready = 0;
  return size;
}

uint8_t tx_q_add (char *msg, uint8_t size, uint8_t priority)
{
  int i, j, found;

// Insert into FIFO queue
  j = q_index;
  found = 0;
  for (i = 0; i < MAX_Q_LEN; i++) {
    if (tx_q[j].ready == 0) {
      found = 1;
      break;
    }
    j++;
    if (j >= MAX_Q_LEN)
      j = 0;
  }
// No room left in queue
  if (found == 0)
    return 0;

  for (i = 0; i < size; i++)
    tx_q[j].pkt[i] = msg[i];
  tx_q[j].size = size;
  tx_q[j].priority = priority;
  tx_q[j].ready = 1;

  return 1;
}
