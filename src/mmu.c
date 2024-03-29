#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <fxCG50gdb/bswap.h>
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
		ret = mmu_translate_address_real_context(virtual_address, real_cpu_pteh() & 0xff);
	}
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_translate_address : va=0x%08X pa=0x%08X mmucr_at=%d no_mmu=%d\n", virtual_address, ret,
			 mmucr_at, no_mmu);
#endif
	return ret;
}

static void mmu_update_instruction_cache(uint32_t* dword_ptr) {
	real_decode_instruction decode_instruction = real_cpu_decode_instruction();
	uint16_t* word_ptr = (uint16_t*)dword_ptr;
	uint16_t* cache_ptr = (uint16_t*)((uint8_t*)dword_ptr + MMU_REGION_SIZE);
	for (size_t i = 0; i < 2; i++) {
		cache_ptr[i] = (decode_instruction(word_ptr[i]) & 0xFF) << 8;
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_update_instruction_cache : i@0x%p ic@0x%p i=0x%02X ic=0x%02X\n",
				 (void*)&word_ptr[i], (void*)&cache_ptr[i], word_ptr[i], cache_ptr[i]);
#endif
	}
}

// This is stored as an array of little-endian 32 bits values, we should read them separately and convert them back to
// big-endian
static uint32_t mmu_read_physical_dword(uint32_t physical_address) {
	assert((physical_address & 3) == 0);
	if (physical_address > MMU_P4_START) {
		struct mmu_region* region = mmu_get_region(physical_address);
		uint32_t* data = (uint32_t*)((uint32_t)region->data & 0xfffffffc);
		void* function = region->module_functions[13];
		uint32_t value = mmu_rw_dword_real_context(physical_address, physical_address, 0,
							   &data[(physical_address & 0xfff) >> 2],
							   region->module_functions, function);
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_read_physical_dword : pa=0x%08X d@0x%p f@0x%p v=0x%08X\n", physical_address,
				 (void*)data, function, value);
#endif
		return value;
	}

	uint32_t* data = (uint32_t*)((uint32_t)mmu_get_region(physical_address)->data & 0xfffffffc);
	uint32_t value = data == NULL ? 0xFFFFFFFF : data[(physical_address & 0xfff) >> 2];
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_read_physical_dword : pa=0x%08X d@0x%p v=0x%08X\n", physical_address, (void*)data, value);
#endif
	return value;
}

static void mmu_write_physical_dword(uint32_t physical_address, uint32_t value) {
	assert((physical_address & 3) == 0);
	if (physical_address > MMU_P4_START) {
		struct mmu_region* region = mmu_get_region(physical_address);
		uint32_t* data = (uint32_t*)((uint32_t)region->data & 0xfffffffc);
		uint32_t* dword_ptr = &data[(physical_address & 0xfff) >> 2];
		void* function = region->module_functions[17];
		mmu_rw_dword_real_context(physical_address, physical_address, value, dword_ptr,
					  region->module_functions, function);
		// We shouldn't update the instruction cache, the module's function will do it correctly
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_write_physical_dword : pa=0x%08X d@0x%p f@0x%p v=0x%08X\n", physical_address,
				 (void*)data, function, value);
#endif
		return;
	}

	uint32_t* data = (uint32_t*)((uint32_t)mmu_get_region(physical_address)->data & 0xfffffffc);
	if (data != NULL)
		data[(physical_address & 0xfff) >> 2] = value;
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_write_physical_dword : pa=0x%08X d@0x%p v=0x%08X\n", physical_address, (void*)data,
			 value);
#endif
	mmu_update_instruction_cache(&data[(physical_address & 0xfff) >> 2]);
}

static uint32_t mmu_read_virtual_dword(uint32_t virtual_address) {
	return mmu_read_physical_dword(mmu_translate_address(virtual_address));
}

static void mmu_write_virtual_dword(uint32_t virtual_address, uint32_t value) {
	mmu_write_physical_dword(mmu_translate_address(virtual_address), value);
}

#ifdef MMU_DIS_CACHE
static uint8_t mmu_dis_cache_buffer[MMU_REGION_SIZE];
static uint32_t mmu_dis_cache_base_virtual_address;
static bool mmu_dis_cache_loaded;

void mmu_dis_cache_unload(void) {
	mmu_dis_cache_loaded = false;
}

static void mmu_dis_cache_load(uint32_t virtual_address) {
	mmu_dis_cache_base_virtual_address = virtual_address & 0xfffff000;
	struct mmu_region* region = mmu_get_region(mmu_translate_address(mmu_dis_cache_base_virtual_address));
	uint32_t* data = (uint32_t*)((uint32_t)region->data & 0xfffffffc);
	memcpy(mmu_dis_cache_buffer, data, MMU_REGION_SIZE);

	for (size_t i = 0; i < MMU_REGION_SIZE / sizeof(uint32_t); i++) {
		((uint32_t*)mmu_dis_cache_buffer)[i] = htoel(((uint32_t*)mmu_dis_cache_buffer)[i]);
	}

	mmu_dis_cache_loaded = true;
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_dis_cache_load : loaded disassembler cache va=0x%08X\n",
			 mmu_dis_cache_base_virtual_address);
#endif
}

static bool mmu_dis_cache_is_loaded(uint32_t virtual_address) {
	if (!mmu_dis_cache_loaded)
		return false;
	if ((virtual_address & 0xfffff000) == mmu_dis_cache_base_virtual_address)
		return true;
	return false;
}
#endif

void mmu_read(uint32_t virtual_address, uint8_t* buf, size_t size, size_t back_size, uint8_t** real_start) {
	// Kinda ugly but 16 should be enough to take into account the alignment of the address and size rounding
	assert(back_size >= size + 16);

#ifdef MMU_DIS_CACHE
	if (size == 2 && (virtual_address & 1) == 0) {
		if (!mmu_dis_cache_is_loaded(virtual_address))
			mmu_dis_cache_load(virtual_address);
		uint16_t value = ((uint16_t*)mmu_dis_cache_buffer)[(virtual_address & 0xfff) >> 1];
		((uint16_t*)buf)[0] = value;
		*real_start = buf;
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_read : hit disassembler cache va=0x%08X v=0x%04X\n", virtual_address, value);
#endif
		return;
	} else if (mmu_dis_cache_is_loaded(virtual_address) && mmu_dis_cache_is_loaded(virtual_address + size - 1)) {
		memcpy(buf, mmu_dis_cache_buffer + (virtual_address & 0xfff), size);
		*real_start = buf;
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_read : hit disassembler cache va=0x%08X s=%d\n", virtual_address, size);
#endif
		return;
	}
#endif

	uint32_t aligned_address = virtual_address & 0xfffffffc;

#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_read : va=0x%08X aa=0x%08X s=%d\n", virtual_address, aligned_address, size);
#endif
	size_t i = 0;
	uint32_t a = aligned_address;
	for (; a < (virtual_address + size); i++, a += 4) {
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_read : a=0x%08X i=%d\n", a, i);
#endif
		((uint32_t*)buf)[i] = htoel(mmu_read_virtual_dword(a));
	}
	*real_start = buf + (virtual_address - aligned_address);
#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_read : b@0x%p rs@0x%p s=%d\n", (void*)buf, (void*)*real_start, size);
#endif
}

void mmu_write(uint32_t virtual_address, uint8_t* buf, size_t size) {
#ifdef MMU_DIS_CACHE
	if (mmu_dis_cache_is_loaded(virtual_address))
		mmu_dis_cache_unload();
#endif

	uint32_t aligned_address = virtual_address & 0xfffffffc;
	// Kinda ugly but 16 should be enough to take into account the alignment of the address and size rounding
	uint8_t* local_buf = malloc(size + 16);
	memcpy(local_buf + (virtual_address - aligned_address), buf, size);

	// There is probably a better way to do this but it works
	// We just fill local_buf with the extra bytes before and after the provided ones to make everything aligned
	if (virtual_address != aligned_address) {
		uint32_t old_value = htoel(mmu_read_virtual_dword(aligned_address));
		uint8_t* b = (uint8_t*)&old_value;
		size_t i = 0;
		for (; (unsigned int)(b - (uint8_t*)&old_value) < virtual_address - aligned_address; b++, i++) {
			local_buf[i] = *b;
		}
	}
	size_t high_part_size = (virtual_address - aligned_address) + size;
	if (high_part_size % 4 != 0) {
		uint32_t last_aligned_address = (virtual_address + size) & 0xfffffffc;
		uint32_t old_value = htoel(mmu_read_virtual_dword(last_aligned_address));
		uint8_t* b = (uint8_t*)&old_value;
		b += high_part_size % 4;
		size_t i = high_part_size;
		for (; (unsigned int)(b - (uint8_t*)&old_value) < 4; b++, i++) {
			local_buf[i] = *b;
		}
	}

#ifdef MMU_DEBUG
	fxCG50gdb_printf("mmu_write : lb@0x%p va=0x%08X aa=0x%08X s=%d\n", (void*)local_buf, virtual_address,
			 aligned_address, size);
#endif
	uint32_t* local_buf_as_dword = (uint32_t*)local_buf;
	size_t i = 0;
	uint32_t a = aligned_address;
	for (; a < (virtual_address + size); i++, a += 4) {
#ifdef MMU_DEBUG
		fxCG50gdb_printf("mmu_write : a=%08X i=%d\n", a, i);
#endif
		mmu_write_virtual_dword(a, etohl(local_buf_as_dword[i]));
	}
	free(local_buf);
}
