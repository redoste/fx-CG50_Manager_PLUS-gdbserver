bits 32
section .text

extern _real_cpu_translate_address_ptr

; uint32_t mmu_translate_address_real_context(uint32_t virtual_address);
global _mmu_translate_address_real_context
_mmu_translate_address_real_context:
	mov eax, [esp + 4]
	pushad

	; Special registers that *might* be used by emulator's code
	mov ebp, 0xA0000000 ; Emulated CPU PC
	mov edi, 0xDEADC0DE ; Pointer to current instruction in host's address space

	mov ebx, [_real_cpu_translate_address_ptr]
	call ebx
	mov [eax_backup], eax

	popad
	mov eax, [eax_backup]
	ret

; uint32_t mmu_read_dword_real_context(uint32_t virtual_address, uint32_t physical_address, uint32_t *data_ptr, void *module_functions, void *function);
global _mmu_read_dword_real_context
_mmu_read_dword_real_context:
	pushad

	mov eax, [esp + 32 + 20]
	mov [eax_backup], eax

	; Special registers that *might* be used by emulator's code
	mov eax, [esp + 32 + 4] ; virtual_address
	mov ecx, [esp + 32 + 8] ; physical_address
	mov ebx, [esp + 32 + 12] ; pointer to the corresponding dword in the data section
	mov esi, [esp + 32 + 16] ; pointer to the module functions table

	; Special registers that *might* be used by emulator's code
	mov ebp, 0xA0000000 ; Emulated CPU PC
	mov edi, 0xDEADC0DE ; Pointer to current instruction in host's address space

	call [eax_backup]
	mov [eax_backup], eax

	popad
	mov eax, [eax_backup]
	ret

section .data
eax_backup: dw 0
