#include <stdio.h>
#include <windows.h>
#include <winsock2.h>

#include <fxCG50gdb/break.h>
#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/gdb.h>
#include <fxCG50gdb/stdio.h>

void break_main(struct break_state* context) {
	fxCG50gdb_printf("BREAK with context at 0x%p\n", context);

	struct registers* r = real_cpu_registers();
	r->pc = context->ebp;
	fxCG50gdb_printf("PC = %08X\n", r->pc);
	fxCG50gdb_printf("r0 = %08X r1 = %08X r2 = %08X r3 = %08X\n", r->r0, r->r1, r->r2, r->r3);
	fxCG50gdb_printf("r4 = %08X r5 = %08X r6 = %08X r7 = %08X\n", r->r4, r->r5, r->r6, r->r7);
	fxCG50gdb_printf("r8 = %08X r9 = %08X rA = %08X rB = %08X\n", r->r8, r->r9, r->r10, r->r11);
	fxCG50gdb_printf("rC = %08X rD = %08X rE = %08X rF = %08X\n", r->r12, r->r13, r->r14, r->r15);

	gdb_main(true);
}

void* break_jti_original_function_ptr;
void break_jti_main(struct break_state* context) {
	if (context->eax & 0x80)
		break_jti_original_function_ptr = real_cpu_jti_table_wtmmu_backup[context->eax & 0x7f];
	else
		break_jti_original_function_ptr = real_cpu_jti_table_nommu_backup[context->eax & 0x7f];
}
