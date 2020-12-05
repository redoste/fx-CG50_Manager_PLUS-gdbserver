/* THIS FILE CONTAINS HARDCODED OFFSETS FOR A SPECIFIC VERSION OF CPU73050.dll
 * $ sha1sum CPU73050.dll
 * b02ea91060224da0595ecd56b6529f241b4e8295  CPU73050.dll
 */
#ifndef EMULATOR_OFFSETS_H
#define EMULATOR_OFFSETS_H

#include <stdint.h>

static const size_t real_dll_registers_off = 0x675918;
static const size_t real_dll_instruction_table_off = 0x93190;
static const size_t real_dll_instruction_table_break_index = 228;
static const size_t real_dll_instruction_table_break_amount = 28;

static const size_t real_dll_next_instruction_ptr_off = 0x368c10;
static const size_t real_dll_read_byte_ptr_off = 0x368bf4;

static const size_t real_dll_read_byte_exception_patch_off = 0x9f;
static const uint8_t real_dll_read_byte_exception_patch[] = {
	0x31, 0xc0,  // xor    eax,eax
	0xb0, 0xff,  // mov    al,0xff
	0xc3	     // ret
};
static const size_t real_dll_read_byte_expected_function = 0x27f62;
#endif
