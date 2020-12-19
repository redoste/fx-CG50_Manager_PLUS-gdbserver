/* THIS FILE CONTAINS HARDCODED OFFSETS FOR A SPECIFIC VERSION OF CPU73050.dll
 * $ sha1sum CPU73050.dll
 * b02ea91060224da0595ecd56b6529f241b4e8295  CPU73050.dll
 */
#ifndef EMULATOR_OFFSETS_H
#define EMULATOR_OFFSETS_H

#include <stdint.h>

static const size_t real_dll_registers_off = 0x675918;
static const size_t real_dll_mmucr_off = 0x67be88;
static const size_t real_dll_mmu_regions_off = 0xcefe0;
static const size_t real_dll_mmu_regions_p4_off = 0x371020;
static const size_t real_dll_mmu_no_translation_table = 0x34f3b8;
static const size_t real_dll_instruction_table_off = 0x93190;
static const size_t real_dll_instruction_table_break_index = 228;
static const size_t real_dll_instruction_table_break_amount = 28;
static const size_t real_dll_instruction_table_trapa_index = 145;
static const size_t real_dll_jmp_to_instruction_table_wtmmu_off = 0x94390;
static const size_t real_dll_jmp_to_instruction_table_nommu_off = 0x94328;
static const size_t real_dll_jmp_to_instruction_table_amount = 11;

static const size_t real_dll_next_instruction_ptr_off = 0x368c10;
static const size_t real_dll_mmu_translate_address = 0x21d8a;
static const size_t real_dll_decode_instruction_off = 0x16050;
#endif
