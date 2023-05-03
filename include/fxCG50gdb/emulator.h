#ifndef EMULATOR_H
#define EMULATOR_H

#include <stdint.h>
#include <windows.h>

// I know, this structure is beautiful
// I should have used an array but I don't want that the compiler mess things up for alignement and if I found out about
// other registers it will be easier to add them.
struct registers {
	uint32_t r0;
	uint32_t unk0;
	uint32_t r1;
	uint32_t unk1;
	uint32_t r2;
	uint32_t unk2;
	uint32_t r3;
	uint32_t unk3;
	uint32_t r4;
	uint32_t unk4;
	uint32_t r5;
	uint32_t unk5;
	uint32_t r6;
	uint32_t unk6;
	uint32_t r7;
	uint32_t unk7;
	uint32_t r8;
	uint32_t unk8;
	uint32_t r9;
	uint32_t unk9;
	uint32_t r10;
	uint32_t unk10;
	uint32_t r11;
	uint32_t unk11;
	uint32_t r12;
	uint32_t unk12;
	uint32_t r13;
	uint32_t unk13;
	uint32_t r14;
	uint32_t unk14;
	uint32_t r15;
	uint32_t unk15;
	uint32_t sr;
	uint32_t unk16;
	uint32_t gbr;
	uint32_t unk17;
	uint32_t vbr;
	uint32_t unk18;
	uint32_t ssr;
	uint32_t unk19;
	uint32_t spc;
	uint32_t unk20;
	uint32_t unk21;
	uint32_t unk22;
	uint32_t unk23;
	uint32_t unk24;
	uint32_t unk25;
	uint32_t unk26;
	uint32_t unk27;
	uint32_t unk28;
	uint32_t unk29;
	uint32_t unk30;
	uint32_t unk31;
	uint32_t unk32;
	uint32_t unk33;
	uint32_t unk34;
	uint32_t unk35;
	uint32_t unk36;
	uint32_t mach;
	uint32_t unk37;
	uint32_t macl;
	uint32_t unk38;
	uint32_t pr;
	uint32_t unk39;
	uint32_t pc;
};

typedef void*(__stdcall* real_DLDriver)(void);
typedef uint16_t(__stdcall* real_decode_instruction)(uint16_t);

extern HINSTANCE real_cpu_dll;
extern void* real_cpu_translate_address_ptr;
extern void** real_cpu_jti_table_nommu_backup;
extern void** real_cpu_jti_table_wtmmu_backup;

struct registers* real_cpu_registers(void);
void real_cpu_init(void);
real_DLDriver real_DLDriverInfo(void);
real_DLDriver real_DLDriverInfoCall(void);
uint32_t real_cpu_mmucr(void);
struct mmu_region* real_cpu_mmu_regions(void);
struct mmu_region* real_cpu_mmu_regions_p4(void);
uint32_t* real_cpu_mmu_no_translation_table(void);
real_decode_instruction real_cpu_decode_instruction(void);
void* real_cpu_next_instruction_function(void);
void real_cpu_clean_delayed_branch(void);
void* real_cpu_instruction_table_function(size_t index);
void (**real_cpu_SCFTDR_handlers(void))(void);

#endif
