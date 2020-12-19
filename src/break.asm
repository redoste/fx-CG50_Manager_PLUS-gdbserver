bits 32
section .text

extern _break_ii_main
extern _break_jti_main
extern _break_ii_next_function_ptr
extern _break_jti_original_function_ptr

global _break_ii_handler
_break_ii_handler:
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

	call _break_ii_main

	mov esp, ebp
	popad

	jmp [_break_ii_next_function_ptr]

global _break_jti_handler
_break_jti_handler:
	pushad
	mov ebp, esp

	push ebp

	; Same stack setup as _break_ii_handler

	call _break_jti_main

	mov esp, ebp
	popad

	jmp [_break_jti_original_function_ptr]
