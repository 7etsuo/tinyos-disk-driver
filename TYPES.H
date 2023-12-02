#ifndef TYPES_H
#define TYPES_H

typedef unsigned char UINT8;
typedef unsigned int UINT16;
typedef unsigned long UINT32;

typedef void (*Vector)();

typedef volatile UINT8 *const IO_PORT8;
typedef volatile UINT16 *const IO_PORT16;
typedef volatile const UINT8 *const IO_PORT8_RO;
typedef volatile const UINT16 *const IO_PORT16_RO;

#endif