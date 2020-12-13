#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <winsock2.h>

#include <fxCG50gdb/emulator.h>
#include <fxCG50gdb/mmu.h>
#include <fxCG50gdb/stdio.h>

struct mmu_region* mmu_get_region(uint32_t physical_address) {
	uint32_t region = (physical_address >> 0xc) & 0x1ffff;
	if (physical_address < MMU_P4_START) {
		return &(real_cpu_mmu_regions()[region]);
	} else {
		return &(real_cpu_mmu_regions_p4()[region]);
	}
}

uint32_t mmu_translate_address(uint32_t virtual_address) {
	uint32_t ret;
	unsigned int mmucr_at = real_cpu_mmucr() & 1;
	unsigned int no_mmu = (virtual_address >= MMU_P1_START && virtual_address < MMU_P3_START) ||
			      virtual_address >= MMU_P4_START || !mmucr_at;
	if (no_mmu) {
		ret = real_cpu_mmu_no_translation_table()[virtual_address >> 0x18] | (virtual_address & 0xffffff);
	} else {
		ret = mmu_translate_address_real_context(virtual_address);
	}
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_translate_address : va=0x%08X pa=0x%08X mmucr_at=%d no_mmu=%d\n", virtual_address, ret,
			 mmucr_at, no_mmu);
#endif
	return ret;
}

// This is stored as an array of little-endian 32 bits values, we should read them separately and convert them back to
// big-endian
static uint32_t mmu_read_physical_dword(uint32_t physical_address) {
	assert((physical_address & 3) == 0);
	if (physical_address > MMU_P4_START) {
		struct mmu_region* region = mmu_get_region(physical_address);
		uint32_t* data = (uint32_t*)((uint32_t)region->data & 0xfffffffc);
		void* function = region->module_functions[13];
		uint32_t value = mmu_read_dword_real_context(physical_address, physical_address,
							     &data[(physical_address & 0xfff) >> 2],
							     region->module_functions, function);
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_read_physical_dword : pa=0x%08X d@0x%08X f@0x%08X v=0x%08X\n", physical_address,
				 data, function, value);
#endif
		return value;
	}

	uint32_t* data = (uint32_t*)((uint32_t)mmu_get_region(physical_address)->data & 0xfffffffc);
	uint32_t value = data == NULL ? 0xFFFFFFFF : data[(physical_address & 0xfff) >> 2];
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_read_physical_dword : pa=0x%08X d@0x%08X v=0x%08X\n", physical_address, data, value);
#endif
	return value;
}

static uint32_t mmu_read_virtual_dword(uint32_t virtual_address) {
	return mmu_read_physical_dword(mmu_translate_address(virtual_address));
}

void mmu_read(uint32_t virtual_address, uint8_t* buf, size_t size) {
	uint32_t aligned_address = virtual_address & 0xfffffffc;
	// Kinda ugly but 16 should be enough to take into account the alignment of the address and size rounding
	uint32_t* local_buf = malloc(size + 16);

#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_read : va=0x%08X aa=0x%08X s=%d\n", virtual_address, aligned_address, size);
#endif
	size_t i = 0;
	uint32_t a = aligned_address;
	for (; a < (virtual_address + size); i++, a += 4) {
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_read : a=0x%08X i=%d\n", a, i);
#endif
		local_buf[i] = htonl(mmu_read_virtual_dword(a));
	}
	uint8_t* copy_start = (uint8_t*)local_buf + (virtual_address - aligned_address);
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_read : lb@0x%08X cs@0x%08X s=%d\n", local_buf, copy_start, size);
#endif
	memcpy(buf, copy_start, size);
	free(local_buf);
}
