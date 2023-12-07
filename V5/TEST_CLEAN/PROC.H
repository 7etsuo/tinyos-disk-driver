/* process.h */
#ifndef PROCESS_H
#define PROCESS_H

#include "TYPES.H"

void create_process(UINT16 prog_num, UINT16 is_fg);
void schedule();
void terminate();
void load(UINT16 i, void (*p)());

#endif /* PROCESS_H */
