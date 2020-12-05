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
void* real_cpu_read_byte_ptr;

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
}

uint8_t real_cpu_read(uint32_t address) {
	real_cpu_read_byte_ptr = ((uint8_t*)real_cpu_dll + real_dll_read_byte_ptr_off);

	// A patch is applied to prevent the MMU to trigger exceptions when reading invalid addresses
	// For the moment this patch only works when a certain function is used to read
	uint8_t* function = *((uint8_t**)real_cpu_read_byte_ptr);
	assert(function == ((uint8_t*)real_cpu_dll + real_dll_read_byte_expected_function));

	size_t patch_size = sizeof(real_dll_read_byte_exception_patch);
	uint8_t* backup = malloc(patch_size);
	uint8_t* to_patch = function + real_dll_read_byte_exception_patch_off;
	DWORD old_protection;

	VirtualProtect(to_patch, patch_size, PAGE_EXECUTE_READWRITE, &old_protection);
	memcpy(backup, to_patch, patch_size);
	memcpy(to_patch, real_dll_read_byte_exception_patch, patch_size);
	uint8_t ret = real_cpu_read_real_context(address);
	memcpy(to_patch, backup, patch_size);
	VirtualProtect(to_patch, patch_size, old_protection, &old_protection);
	free(backup);
	return ret;
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
