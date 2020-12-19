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

extern void* break_ii_next_function_ptr;
extern void* break_jti_original_function_ptr;

void break_ii_handler();
void break_jti_handler();
void break_main(struct break_state*);
void break_ii_main(struct break_state*);
void break_jti_main(struct break_state*);

#endif
