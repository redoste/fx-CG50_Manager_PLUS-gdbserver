#ifndef MMU_H
#define MMU_H

#include <stdint.h>

// Comment this line to disable MMU debug messages
//#define MMU_DEBUG
// Comment this line to disable MMU disassembler cache
#define MMU_DIS_CACHE

#define MMU_P0_START 0x00000000
#define MMU_P1_START 0x80000000
#define MMU_P2_START 0xA0000000
#define MMU_P3_START 0xC0000000
#define MMU_P4_START 0xE0000000

#define MMU_REGION_SIZE 0x1000

struct mmu_region {
	void** module_functions;
	uint32_t* data;
};

struct mmu_region* mmu_get_region(uint32_t physical_address);
uint32_t mmu_translate_address(uint32_t virtual_address);
uint32_t mmu_translate_address_real_context(uint32_t virtual_address, uint8_t asid);
uint32_t mmu_rw_dword_real_context(uint32_t virtual_address,
				   uint32_t physical_address,
				   uint32_t value,
				   uint32_t* data_ptr,
				   void* module_functions,
				   void* function);
#ifdef MMU_DIS_CACHE
void mmu_dis_cache_unload(void);
#endif
void mmu_read(uint32_t virtual_address, uint8_t* buf, size_t size, size_t back_size, uint8_t** real_start);
void mmu_write(uint32_t virtual_address, uint8_t* buf, size_t size);

#endif
