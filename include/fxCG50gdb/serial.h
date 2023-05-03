#ifndef SERIAL_H
#define SERIAL_H

#include <stdio.h>

void serial_init(void);
void serial_handler(void);

extern FILE* serial_output_file;
extern void (*serial_old_handler)(void);

#endif
