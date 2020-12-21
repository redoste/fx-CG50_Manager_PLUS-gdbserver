bits 32
section .text

extern _break_ii_main
extern _break_jti_main
extern _break_ii_next_function_ptr
extern _break_jti_original_function_ptr

extern _gdb_wants_step
extern _gdb_breakpoints_amount
extern _real_cpu_jti_table_nommu_backup
extern _real_cpu_jti_table_wtmmu_backup

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
	cmp BYTE [_gdb_wants_step], 0
	jne .slow_path
	cmp DWORD [_gdb_breakpoints_amount], 0
	jne .slow_path

.fast_path:
	push eax
	and al, 0x80
	cmp al, 0
	pop eax
	push ebx
	jne .fast_path_wtmmu
.fast_path_nommu:
	mov ebx, [_real_cpu_jti_table_nommu_backup]
	jmp .fast_path_end
.fast_path_wtmmu:
	mov ebx, [_real_cpu_jti_table_wtmmu_backup]
	and eax, 0x7f
.fast_path_end:
	mov eax, [ebx + eax*4]
	pop ebx
	jmp eax

.slow_path:
	pushad
	mov ebp, esp

	push ebp

	; Same stack setup as _break_ii_handler

	call _break_jti_main

	mov esp, ebp
	popad

	jmp [_break_jti_original_function_ptr]
