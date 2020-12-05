#ifndef BREAK_H
#define BREAK_H

#include <stdint.h>

struct break_state {
	uint32_t edi;
	uint32_t esi;
	uint32_t ebp;
	uint32_t esp;
	uint32_t ebx;
	uint32_t edx;
	uint32_t ecx;
	uint32_t eax;
};

void break_handler();
void break_main(struct break_state*);

#endif
