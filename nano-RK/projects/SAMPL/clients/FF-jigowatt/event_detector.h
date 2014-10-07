#ifndef EVENT_DETECTOR_H_
#define EVENT_DETECTOR_H_
#include <nrk.h>

#define EVENT_DETECTOR_STACKSIZE	256	
NRK_STK event_detector_stack[NRK_APP_STACKSIZE];
nrk_task_type event_detector;
void event_detector_task();

#endif
