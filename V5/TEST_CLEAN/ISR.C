#include "ISR.H"
#include "CONS.H"
#include "PROC.H"
#include "UTILS.H"

void vbl_isr()
{
    /* ... vertical blank interrupt service routine ... */
}

void do_vbl_isr()
{
    /* ... do vertical blank ISR code ... */
}

void exception_isr()
{
    /* ... exception ISR code ... */
}

void do_exception_isr(UINT16 sr)
{
    /* ... do exception ISR code ... */
}

void addr_exception_isr()
{
    /* ... address exception ISR code ... */
}

void do_addr_exception_isr(UINT16 flags, UINT32 addr, UINT16 ir, UINT16 sr)
{
    /* ... do address exception ISR code ... */
}

void timer_A_isr()
{
    /* ... Timer A ISR code ... */
}

void do_timer_A_isr(UINT16 sr)
{
    /* ... do Timer A ISR code ... */
}

void ikbd_isr()
{
    /* ... IKBD ISR code ... */
}

void input_enqueue(char ch)
{
    /* ... input enqueue code ... */
}
