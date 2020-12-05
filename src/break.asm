bits 32
section .text

extern _break_main
extern _real_cpu_next_instruction_ptr

global _break_handler
_break_handler:
	pushad
	mov ebp, esp

	push ebp

	;   STACK :
	;
	; Low address
	; +---------+
	; |*context | <- ESP
	; +---------+
	; | OLD EDI | <- EBP
	; +---------+
	; | OLD ESI |
	; +---------+
	; | OLD EBP |
	; +---------+
	; | OLD ESP |
	; +---------+
	; | OLD EBX |
	; +---------+
	; | OLD EDX |
	; +---------+
	; | OLD ECX |
	; +---------+
	; | OLD EAX |
	; +---------+
	; |   ...   |
	; +---------+
	; High address

	call _break_main

	mov esp, ebp
	popad

	mov eax, [_real_cpu_next_instruction_ptr]
	jmp [eax]
