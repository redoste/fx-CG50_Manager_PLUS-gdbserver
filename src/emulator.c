#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <fxCG50gdb/break.h>
#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/emulator_offsets.h>
#include <fxCG50gdb/stdio.h>

HINSTANCE real_cpu_dll;
void* real_cpu_next_instruction_ptr;
void* real_cpu_translate_address_ptr;

struct registers* real_cpu_registers() {
	return (struct registers*)((uint8_t*)real_cpu_dll + real_dll_registers_off);
}

void real_cpu_hijack_break() {
	void (**instruction_table)() = (void (**)())((uint8_t*)real_cpu_dll + real_dll_instruction_table_off);
	fxCG50gdb_printf("Hijacking break... Before at 0x%p\n",
			 instruction_table[real_dll_instruction_table_break_index]);
	for (size_t i = 0; i < real_dll_instruction_table_break_amount; i++) {
		instruction_table[real_dll_instruction_table_break_index + i] = &break_handler;
	}
	fxCG50gdb_printf("Break hijacked. Now at 0x%p\n", instruction_table[real_dll_instruction_table_break_index]);

	real_cpu_next_instruction_ptr = ((uint8_t*)real_cpu_dll + real_dll_next_instruction_ptr_off);
	real_cpu_translate_address_ptr = ((uint8_t*)real_cpu_dll + real_dll_mmu_translate_address);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
real_DLDriver real_DLDriverInfo() {
	return (real_DLDriver)GetProcAddress(real_cpu_dll, "_DLDriverInfo@0");
}

real_DLDriver real_DLDriverInfoCall() {
	return (real_DLDriver)GetProcAddress(real_cpu_dll, "_DLDriverInfoCall@0");
}
#pragma GCC diagnostic pop

uint32_t real_cpu_mmucr() {
	return *(uint32_t*)((uint8_t*)real_cpu_dll + real_dll_mmucr_off);
}

struct mmu_region* real_cpu_mmu_regions() {
	return (struct mmu_region*)((uint8_t*)real_cpu_dll + real_dll_mmu_regions_off);
}

struct mmu_region* real_cpu_mmu_regions_p4() {
	return (struct mmu_region*)((uint8_t*)real_cpu_dll + real_dll_mmu_regions_p4_off);
}

uint32_t* real_cpu_mmu_no_translation_table() {
	return (uint32_t*)((uint8_t*)real_cpu_dll + real_dll_mmu_no_translation_table);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
real_decode_instruction real_cpu_decode_instruction() {
	return (real_decode_instruction)((uint8_t*)real_cpu_dll + real_dll_decode_instruction_off);
}
#pragma GCC diagnostic pop
