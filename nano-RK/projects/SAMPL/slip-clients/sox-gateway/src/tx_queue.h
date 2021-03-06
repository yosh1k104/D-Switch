#ifndef _TX_QUEUE_H_
#define _TX_QUEUE_H_

#include <stdint.h>


#define MAX_Q_LEN	10

typedef struct queue_element {
  uint8_t pkt[113];
  uint8_t size;
  uint8_t ready;
  // Higher number is a higher priority
  uint8_t priority;
} TX_Q_ELEMENT_T;

uint8_t q_index;

TX_Q_ELEMENT_T tx_q[MAX_Q_LEN];

void tx_q_init(void);
uint8_t tx_q_pending();
uint8_t tx_q_get(char *msg);
uint8_t tx_q_add(char *msg, uint8_t size, uint8_t priority);
#endif
