bits 32
section .text

extern _serial_output_file
extern _serial_old_handler
extern _putc

global _serial_handler
_serial_handler:
	push eax
	push ecx
	push edx

	mov eax, edx
	and eax, 0xff

	push dword [_serial_output_file]
	push eax
	call _putc
	add esp, 8

	pop edx
	pop ecx
	pop eax
	jmp [_serial_old_handler]
