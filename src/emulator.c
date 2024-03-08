#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>

#include <fxCG50gdb/break.h>
#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/emulator_offsets.h>
#include <fxCG50gdb/serial.h>
#include <fxCG50gdb/stdio.h>

HINSTANCE real_cpu_dll;
void* real_cpu_translate_address_ptr;

struct registers* real_cpu_registers(void) {
	return (struct registers*)((uint8_t*)real_cpu_dll + real_dll_registers_off);
}

static void real_cpu_hijack_break(void) {
	void (**instruction_table)(void) = (void (**)(void))((uint8_t*)real_cpu_dll + real_dll_instruction_table_off);
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
static void real_cpu_hijack_jmp_to_instruction(void) {
	void*** jti_table_nommu = (void***)((uint8_t*)real_cpu_dll + real_dll_jmp_to_instruction_table_nommu_off);
	void*** jti_table_wtmmu = (void***)((uint8_t*)real_cpu_dll + real_dll_jmp_to_instruction_table_wtmmu_off);

	/* We used to erase the function pointers used by the emulator when SR.MD = 0 since CASIOWIN stays in
	 * privileged mode, but now that we support it we backup both function tables
	 */

	const size_t backup_size = sizeof(void*) * real_dll_jmp_to_instruction_table_amount;

	real_cpu_jti_table_nommu_backup = malloc(backup_size * 2);
	memcpy(real_cpu_jti_table_nommu_backup, jti_table_nommu[0], backup_size);
	memcpy((uint8_t*)real_cpu_jti_table_nommu_backup + backup_size, jti_table_nommu[1], backup_size);

	real_cpu_jti_table_wtmmu_backup = malloc(backup_size * 2);
	memcpy(real_cpu_jti_table_wtmmu_backup, jti_table_wtmmu[0], backup_size);
	memcpy((uint8_t*)real_cpu_jti_table_wtmmu_backup + backup_size, jti_table_wtmmu[1], backup_size);

	const uint8_t jmp_slide[] = {
		0x31, 0xC0,		       // xor eax, eax
		0xB0, 0xFF,		       // mov al, 0xff
		0x68, 0xFF, 0xFF, 0xFF, 0xFF,  // push 0xffffffff
		0xC3			       // ret
	};
	const size_t jmp_slide_al_index = 3;
	const size_t jmp_slide_addr_index = 5;
	const size_t slide_size = sizeof(jmp_slide) * real_dll_jmp_to_instruction_table_amount * 4;

	uint8_t* slide = VirtualAlloc(NULL, slide_size, MEM_COMMIT, PAGE_READWRITE);
	uint8_t* a = slide;
	for (size_t i = 0; i < real_dll_jmp_to_instruction_table_amount; i++) {
		/* j & 1 : no mmu or with mmu
		 * j & 2 : user mode or privileged mode
		 */
		for (size_t j = 0; j < 4; j++) {
			memcpy(a, jmp_slide, sizeof(jmp_slide));

			size_t backup_index = i + ((j & 2) ? real_dll_jmp_to_instruction_table_amount : 0);
			a[jmp_slide_al_index] = (uint8_t)(((j & 1) << 7) | (backup_index & 0x7F));

			*((uint32_t*)&a[jmp_slide_addr_index]) = (uint32_t)&break_jti_handler;

			if (j & 1)
				jti_table_wtmmu[(j & 2) >> 1][i] = a;
			else
				jti_table_nommu[(j & 2) >> 1][i] = a;

			a += sizeof(jmp_slide);
		}
	}

	// For some reason VirtualProtect fails if we don't provide somewhere to backup the old protection
	DWORD old_protect;
	VirtualProtect(slide, slide_size, PAGE_EXECUTE_READ, &old_protect);
}

void real_cpu_init(void) {
	real_cpu_hijack_break();
	real_cpu_hijack_jmp_to_instruction();

	real_cpu_translate_address_ptr = ((uint8_t*)real_cpu_dll + real_dll_mmu_translate_address);

	serial_init();
}

#if !defined(_MSC_VER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif
real_DLDriver real_DLDriverInfo(void) {
	return (real_DLDriver)GetProcAddress(real_cpu_dll, "_DLDriverInfo@0");
}

real_DLDriver real_DLDriverInfoCall(void) {
	return (real_DLDriver)GetProcAddress(real_cpu_dll, "_DLDriverInfoCall@0");
}
#if !defined(_MSC_VER)
#pragma GCC diagnostic pop
#endif

uint32_t real_cpu_mmucr(void) {
	return *(uint32_t*)((uint8_t*)real_cpu_dll + real_dll_mmucr_off);
}

struct mmu_region* real_cpu_mmu_regions(void) {
	return (struct mmu_region*)((uint8_t*)real_cpu_dll + real_dll_mmu_regions_off);
}

struct mmu_region* real_cpu_mmu_regions_p4(void) {
	return (struct mmu_region*)((uint8_t*)real_cpu_dll + real_dll_mmu_regions_p4_off);
}

uint32_t* real_cpu_mmu_no_translation_table(void) {
	return (uint32_t*)((uint8_t*)real_cpu_dll + real_dll_mmu_no_translation_table);
}

#if !defined(_MSC_VER)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
real_decode_instruction real_cpu_decode_instruction(void) {
	return (real_decode_instruction)((uint8_t*)real_cpu_dll + real_dll_decode_instruction_off);
}
#if !defined(_MSC_VER)
#pragma GCC diagnostic pop
#endif

void* real_cpu_next_instruction_function(void) {
	void** p = (void**)((uint8_t*)real_cpu_dll + real_dll_next_instruction_ptr_off);
	return *p;
}

void real_cpu_clean_delayed_branch(void) {
	uint32_t* dest = (uint32_t*)((uint8_t*)real_cpu_dll + real_dll_next_instruction_ptr_off);
	uint32_t* src = (uint32_t*)((uint8_t*)real_cpu_dll + real_dll_next_instruction_no_delayed_branch_ptr_off);
	*dest = *src;
}

void* real_cpu_instruction_table_function(size_t index) {
	void** instruction_table = (void**)((uint8_t*)real_cpu_dll + real_dll_instruction_table_off);
	return instruction_table[index];
}

void (**real_cpu_SCFTDR_handlers(void))(void) {
	void (**p)(void) = (void (**)(void))((uint8_t*)real_cpu_dll + real_dll_SCFTDR_handlers_off);
	return p;
}
