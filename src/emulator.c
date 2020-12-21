#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <fxCG50gdb/break.h>
#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/emulator_offsets.h>
#include <fxCG50gdb/stdio.h>

HINSTANCE real_cpu_dll;
void* real_cpu_translate_address_ptr;

struct registers* real_cpu_registers() {
	return (struct registers*)((uint8_t*)real_cpu_dll + real_dll_registers_off);
}

static void real_cpu_hijack_break() {
	void (**instruction_table)() = (void (**)())((uint8_t*)real_cpu_dll + real_dll_instruction_table_off);
	fxCG50gdb_printf("Hijacking break... Before at 0x%08X\n",
			 (uint32_t)instruction_table[real_dll_instruction_table_break_index]);
	for (size_t i = 0; i < real_dll_instruction_table_break_amount; i++) {
		instruction_table[real_dll_instruction_table_break_index + i] = &break_ii_handler;
	}
	fxCG50gdb_printf("Break hijacked. Now at 0x%08X\n",
			 (uint32_t)instruction_table[real_dll_instruction_table_break_index]);
	fxCG50gdb_printf("Hijacking trapa... Before at 0x%08X\n",
			 (uint32_t)instruction_table[real_dll_instruction_table_trapa_index]);
	instruction_table[real_dll_instruction_table_trapa_index] = &break_ii_handler;
	fxCG50gdb_printf("Trapa hijacked. Now at 0x%08X\n",
			 (uint32_t)instruction_table[real_dll_instruction_table_trapa_index]);
}

void** real_cpu_jti_table_nommu_backup;
void** real_cpu_jti_table_wtmmu_backup;
static void real_cpu_hijack_jmp_to_instruction() {
	void*** jti_table_nommu = (void***)((uint8_t*)real_cpu_dll + real_dll_jmp_to_instruction_table_nommu_off);
	void*** jti_table_wtmmu = (void***)((uint8_t*)real_cpu_dll + real_dll_jmp_to_instruction_table_wtmmu_off);
	// We erase the functions pointers used by the emulator when SR.MD = 0 since this shouldn't happen in CASIOWIN
	for (size_t i = 0; i < real_dll_jmp_to_instruction_table_amount; i++) {
		jti_table_nommu[0][i] = (void*)(0xDEADC0DE);
		jti_table_wtmmu[0][i] = (void*)(0xDEADC0DE);
	}
	real_cpu_jti_table_nommu_backup = malloc(sizeof(void*) * real_dll_jmp_to_instruction_table_amount);
	memcpy(real_cpu_jti_table_nommu_backup, jti_table_nommu[1],
	       sizeof(void*) * real_dll_jmp_to_instruction_table_amount);
	real_cpu_jti_table_wtmmu_backup = malloc(sizeof(void*) * real_dll_jmp_to_instruction_table_amount);
	memcpy(real_cpu_jti_table_wtmmu_backup, jti_table_wtmmu[1],
	       sizeof(void*) * real_dll_jmp_to_instruction_table_amount);

	const uint8_t jmp_slide[] = {
		0x31, 0xC0,		       // xor eax, eax
		0xB0, 0xFF,		       // mov al, 0xff
		0x68, 0xFF, 0xFF, 0xFF, 0xFF,  // push 0xffffffff
		0xC3			       // ret
	};
	const size_t jmp_slide_al_index = 3;
	const size_t jmp_slide_addr_index = 5;
	const size_t slide_size = sizeof(jmp_slide) * real_dll_jmp_to_instruction_table_amount * 2;

	uint8_t* slide = VirtualAlloc(NULL, slide_size, MEM_COMMIT, PAGE_READWRITE);
	uint8_t* a = slide;
	for (size_t i = 0; i < real_dll_jmp_to_instruction_table_amount; i++) {
		for (size_t j = 0; j < 2; j++) {
			memcpy(a, jmp_slide, sizeof(jmp_slide));
			a[jmp_slide_al_index] = (j << 7) | (i & 0xFF);
			*((uint32_t*)&a[jmp_slide_addr_index]) = (uint32_t)&break_jti_handler;
			if (j)
				jti_table_wtmmu[1][i] = a;
			else
				jti_table_nommu[1][i] = a;
			a += sizeof(jmp_slide);
		}
	}

	// For some reason VirtualProtect fails if we don't provide somewhere to backup the old protection
	DWORD old_protect;
	VirtualProtect(slide, slide_size, PAGE_EXECUTE_READ, &old_protect);
}

void real_cpu_init() {
	real_cpu_hijack_break();
	real_cpu_hijack_jmp_to_instruction();

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

void* real_cpu_next_instruction_function() {
	void** p = (void**)((uint8_t*)real_cpu_dll + real_dll_next_instruction_ptr_off);
	return *p;
}

void* real_cpu_instruction_table_function(size_t index) {
	void** instruction_table = (void**)((uint8_t*)real_cpu_dll + real_dll_instruction_table_off);
	return instruction_table[index];
}
