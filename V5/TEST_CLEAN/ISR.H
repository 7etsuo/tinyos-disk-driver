#ifndef ISR_H
#define ISR_H

#include "TYPES.H"

void vbl_isr();
void do_vbl_isr();
void exception_isr();
void do_exception_isr(UINT16 sr);
void addr_exception_isr();
void do_addr_exception_isr(UINT16 flags, UINT32 addr, UINT16 ir, UINT16 sr);
void timer_A_isr();
void do_timer_A_isr(UINT16 sr);
void ikbd_isr();
void input_enqueue(char ch);

#endif /* ISR_H */