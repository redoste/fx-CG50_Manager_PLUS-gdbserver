#include <stdio.h>
#include <windows.h>
#include <winsock2.h>

#include <fxCG50gdb/break.h>
#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/gdb.h>
#include <fxCG50gdb/mmu.h>
#include <fxCG50gdb/stdio.h>

void* break_main(struct break_state* context) {
	fxCG50gdb_printf("BREAK with context at 0x%p\n", (void*)context);

	struct registers* r = real_cpu_registers();
	r->pc = context->ebp;
	fxCG50gdb_printf("PC = %08X\n", r->pc);
	fxCG50gdb_printf("r0 = %08X r1 = %08X r2 = %08X r3 = %08X\n", r->r0, r->r1, r->r2, r->r3);
	fxCG50gdb_printf("r4 = %08X r5 = %08X r6 = %08X r7 = %08X\n", r->r4, r->r5, r->r6, r->r7);
	fxCG50gdb_printf("r8 = %08X r9 = %08X rA = %08X rB = %08X\n", r->r8, r->r9, r->r10, r->r11);
	fxCG50gdb_printf("rC = %08X rD = %08X rE = %08X rF = %08X\n", r->r12, r->r13, r->r14, r->r15);

#ifdef MMU_DIS_CACHE
	mmu_dis_cache_unload();
#endif
	gdb_main(true);

	if (context->ebp != r->pc) {
		fxCG50gdb_printf("break_main : resuming with updated PC : 0x%04X => 0x%04X\n", context->ebp, r->pc);
		context->ebp = r->pc;

		// EDI should point to the current instruction in host's address space
		uint32_t physical_pc = mmu_translate_address(r->pc);
		uint8_t* mmu_region_data = (uint8_t*)((uint32_t)mmu_get_region(physical_pc)->data & 0xfffffffc);
		context->edi = (uint32_t)&mmu_region_data[physical_pc & 0xfff];

		// This avoids issues that might occur if the modifiaction of PC is done during a delayed branch.
		// However, the branch will NOT be taken if the modification of PC is made at the wrong time.

		// Example:
		// 	mov.l @r13, r12
		// 	jsr @r12
		// 	mov #1, r4  <-- here we do the `call`
		// 	shll2 r0
		// Because the delayed branch was cleaned and GDB does not seem to support it's restoration, it will
		// fall through to the `shll2` instruction without calling the function pointed by r12.
		real_cpu_clean_delayed_branch();

		uint8_t instruction_index = *(uint16_t*)((uint8_t*)context->edi + MMU_REGION_SIZE) >> 8;
		return real_cpu_instruction_table_function(instruction_index);
	}
	return NULL;
}

/* There are two ways to break :
 * II  : Inside Instruction  : Used by TRAPA and BRK instructions
 * JTI : Jump to instruction : Used by hardware breakpoints and single-stepping
 */

void* break_ii_next_function_ptr;
void break_ii_main(struct break_state* context) {
	uint16_t instruction = *(uint16_t*)context->edi;
	fxCG50gdb_printf("BREAK with II : instruction = 0x%04X\n", instruction);
	void* next_function = break_main(context);
	if (instruction >> 8 == 0xC3) {
		instruction = *(uint16_t*)context->edi;
		uint8_t instruction_index = *(uint16_t*)((uint8_t*)context->edi + MMU_REGION_SIZE) >> 8;
		fxCG50gdb_printf(
			"break_ii_main : resuming from TRAPA with new instruction = 0x%04X instruction_index = "
			"0x%02X\n",
			instruction, instruction_index);
		break_ii_next_function_ptr =
			next_function == NULL ? real_cpu_instruction_table_function(instruction_index) : next_function;
	} else {
		break_ii_next_function_ptr =
			next_function == NULL ? real_cpu_next_instruction_function() : next_function;
	}
}

void* break_jti_original_function_ptr;
void break_jti_main(struct break_state* context) {
	void* next_function = NULL;
	if (context->eax & 0x80)
		break_jti_original_function_ptr = real_cpu_jti_table_wtmmu_backup[context->eax & 0x7f];
	else
		break_jti_original_function_ptr = real_cpu_jti_table_nommu_backup[context->eax & 0x7f];

	if (gdb_wants_step) {
		gdb_wants_step = false;
		fxCG50gdb_printf("BREAK with JTI : gdb_wants_step == true\n");
		next_function = break_main(context);
	} else {
		uint32_t pc = context->ebp >> 1;
		uint8_t prefix = pc >> 24;
		uint32_t suffix = pc & 0xFFFFFF;
		if (gdb_breakpoints[prefix] == NULL)
			return;
		if (gdb_breakpoints[prefix][suffix >> 3] & (1 << (suffix & 7))) {
			fxCG50gdb_printf("BREAK with JTI : breakpoint at 0x%08X\n", context->ebp);
			next_function = break_main(context);
		}
	}
	if (next_function != NULL)
		break_jti_original_function_ptr = next_function;
}
