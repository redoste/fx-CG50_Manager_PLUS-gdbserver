#ifndef MMU_H
#define MMU_H

#include <stdint.h>

// Comment this line to disable MMU debug messages
//#define MMU_DEBUG

#define MMU_P0_START 0x00000000
#define MMU_P1_START 0x80000000
#define MMU_P2_START 0xA0000000
#define MMU_P3_START 0xC0000000
#define MMU_P4_START 0xE0000000

struct mmu_region {
	void** module_functions;
	uint32_t* data;
};

struct mmu_region* mmu_get_region(uint32_t physical_address);
uint32_t mmu_translate_address(uint32_t virtual_address);
uint32_t mmu_translate_address_real_context(uint32_t virtual_address);
uint32_t mmu_read_dword_real_context(uint32_t virtual_address,
				     uint32_t physical_address,
				     uint32_t* data_ptr,
				     void* module_functions,
				     void* function);
void mmu_read(uint32_t virtual_address, uint8_t* buf, size_t size);

#endif
