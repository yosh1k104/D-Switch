#include <avr/io.h>

#include "contiki-conf.h"

void
cpu_init(void)
{
  asm volatile ("clr r1");	/* No longer needed. */
}

extern int __bss_end;

#define STACK_EXTRA 32
static char *cur_break = (char *)(&__bss_end + 1);

/*
 * Allocate memory from the heap. Check that we don't collide with the
 * stack right now (some other routine might later). A watchdog might
 * be used to check if cur_break and the stack pointer meet during
 * runtime.
 */
void *
sbrk(int incr)
{
  char *stack_pointer;

  stack_pointer = (char *)SP;
  stack_pointer -= STACK_EXTRA;
  if(incr > (stack_pointer - cur_break))
    return (void *)-1;          /* ENOMEM */

  void *old_break = cur_break;
  cur_break += incr;
  /*
   * If the stack was never here then [old_break .. cur_break] should
   * be filled with zeros.
  */
  return old_break;
}

// This is the SUSPEND for the OS timer Tick
//void SIG_OUTPUT_COMPARE0( void ) __attribute__ ( ( signal,naked ));
//void SIG_OUTPUT_COMPARE0(void) {
//   asm volatile (
//   "push    r0 \n\t" \
//   "in      r0, __sreg__  \n\t" \ 
//   "push    r0  \n\t" \
//   "push    r1 \n\t" \
//   "push    r2 \n\t" \
//   "push    r3 \n\t" \
//   "push    r4 \n\t" \
//   "push    r5 \n\t" \
//   "push    r6 \n\t" \
//   "push    r7 \n\t" \
//   "push    r8 \n\t" \
//   "push    r9 \n\t" \
//   "push    r10 \n\t" \
//   "push    r11 \n\t" \
//   "push    r12 \n\t" \
//   "push    r13 \n\t" \
//   "push    r14 \n\t" \
//   "push    r15 \n\t" \
//   "push    r16 \n\t" \
//   "push    r17 \n\t" \
//   "push    r18 \n\t" \
//   "push    r19 \n\t" \
//   "push    r20 \n\t" \
//   "push    r21 \n\t" \
//   "push    r22 \n\t" \
//   "push    r23 \n\t" \
//   "push    r24 \n\t" \
//   "push    r25 \n\t" \
//   "push    r26 \n\t" \
//   "push    r27 \n\t" \
//   "push    r28 \n\t" \
//   "push    r29 \n\t" \
//   "push    r30 \n\t" \
//   "push    r31 \n\t" \
//   "lds r26,nrk_cur_task_TCB \n\t" \
//   "lds r27,nrk_cur_task_TCB+1 \n\t" \
//   "in r0,__SP_L__ \n\t" \
//   "st x+, r0 \n\t" \
//   "in r0,__SP_H__ \n\t" \
//   "st x+, r0 \n\t" \
//   "push r1  \n\t" \
//   "lds r26,nrk_kernel_stk_ptr \n\t" \
//   "lds r27,nrk_kernel_stk_ptr+1 \n\t" \
//   "ld r1,-x \n\t" \
//   "out __SP_H__, r27 \n\t" \
//   "out __SP_L__, r26 \n\t" \
//   "ret\n\t" \
//);
//
//
//	
//	return;  	
//} 
//
//// get another task's register
//void SIG_OUTPUT_COMPARE0( void ) __attribute__ ( ( signal,naked ));
//void SIG_OUTPUT_COMPARE0(void) {
//   asm volatile (
//   "lds r27,nrk_high_ready_TCB \n\t" \
//   "lds r27,nrk_high_ready_TCB+1 \n\t" \
//   
//   	//;x points to &OSTCB[x]
//   
//   "ld r28,x+ \n\t" \
//   "out __SP_L__, r28 \n\t" \
//   "ld r29,x+ \n\t" \
//   "out __SP_H__, r29 \n\t" \
//   
//   "pop     r31\n\t" \
//   "pop     r30\n\t" \
//   "pop     r29\n\t" \
//   "pop     r28\n\t" \
//   "pop     r27\n\t" \
//   "pop     r26\n\t" \
//   "pop     r25\n\t" \
//   "pop     r24\n\t" \
//   "pop     r23\n\t" \
//   "pop     r22\n\t" \
//   "pop     r21\n\t" \
//   "pop     r20\n\t" \
//   "pop     r19 \n\t" \
//   "pop     r18 \n\t" \
//   "pop     r17 \n\t" \
//   "pop     r16 \n\t" \
//   "pop     r15 \n\t" \
//   "pop     r14 \n\t" \
//   "pop     r13 \n\t" \
//   "pop     r12 \n\t" \
//   "pop     r11 \n\t" \
//   "pop     r10 \n\t" \
//   "pop     r9 \n\t" \
//   "pop     r8 \n\t" \
//   "pop     r7 \n\t" \
//   "pop     r6 \n\t" \
//   "pop     r5 \n\t" \
//   "pop     r4 \n\t" \
//   "pop     r3 \n\t" \
//   "pop     r2 \n\t" \
//   "pop     r1 \n\t" \
//   "pop     r0 \n\t" \
//   "out __SREG__, r0 \n\t" \
//   "pop     r0 \n\t" \
//	   
//    	"reti \n\t" \ 
//);
//
//
//	
//	return;  	
//} 
