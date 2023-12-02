#ifndef UTILS_H
#define UTILS_H

#include "TYPES.H"

UINT16 set_ipl(UINT16 ipl);
UINT16 read_SR();
void write_SR(UINT16 sr);
void await_interrupt();

#endif /* UTILS_H */
