bits 32
section .text

extern _real_cpu_read_byte_ptr

global _real_cpu_read_real_context
_real_cpu_read_real_context:
	mov eax, [esp + 4]
	pushad

	; Special registers that *might* be used by emulator's code
	mov ebp, 0xA0000000 ; Emulated CPU PC
	mov edi, 0xDEADC0DE ; Pointer to current instruction in host's address space

	mov ebx, [_real_cpu_read_byte_ptr]
	call [ebx]
	mov [eax_backup], eax

	popad
	mov eax, [eax_backup]
	ret

section .data
eax_backup: dw 0
