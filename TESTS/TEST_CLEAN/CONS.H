/* console.h */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "TYPES.H"

void print_char_safe(char ch);
void print_str_safe(char *str);
void print_char(char ch);
void print_str(char *str);
void clear_screen(UINT8 *base);
void plot_glyph(UINT8 ch);

#endif /* CONSOLE_H */
