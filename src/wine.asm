bits 32
section .text

global _wine_socket
_wine_socket:
	push ebx

	mov edx, [esp + 8 + 2*4] ; protocol
	mov ecx, [esp + 8 + 1*4] ; type
	mov ebx, [esp + 8 + 0*4] ; domain
	mov eax, 0x167
	int 0x80

	pop ebx
	ret

global _wine_bind
_wine_bind:
	push ebx

	mov edx, [esp + 8 + 2*4] ; addrlen
	mov ecx, [esp + 8 + 1*4] ; addr
	mov ebx, [esp + 8 + 0*4] ; sockfd
	mov eax, 0x169
	int 0x80

	pop ebx
	ret

global _wine_listen
_wine_listen:
	push ebx

	mov ecx, [esp + 8 + 1*4] ; backlog
	mov ebx, [esp + 8 + 0*4] ; sockfd
	mov eax, 0x16b
	int 0x80

	pop ebx
	ret

global _wine_accept4
_wine_accept4:
	push ebx
	push esi

	mov esi, [esp + 12 + 3*4] ; flags
	mov edx, [esp + 12 + 2*4] ; addrlen
	mov ecx, [esp + 12 + 1*4] ; addr
	mov ebx, [esp + 12 + 0*4] ; sockfd
	mov eax, 0x16c
	int 0x80

	pop esi
	pop ebx
	ret

global _wine_close
_wine_close:
	push ebx

	mov ebx, [esp + 8 + 0*4] ; fd
	mov eax, 0x6
	int 0x80

	pop ebx
	ret

global _wine_recvfrom
_wine_recvfrom:
	push ebx
	push esi
	push edi
	push ebp

	mov ebp, [esp + 20 + 5*4] ; addrlen
	mov edi, [esp + 20 + 4*4] ; src_addr
	mov esi, [esp + 20 + 3*4] ; flags
	mov edx, [esp + 20 + 2*4] ; len
	mov ecx, [esp + 20 + 1*4] ; buf
	mov ebx, [esp + 20 + 0*4] ; sockfd
	mov eax, 0x173
	int 0x80

	pop ebp
	pop edi
	pop esi
	pop ebx
	ret

global _wine_sendto
_wine_sendto:
	push ebx
	push esi
	push edi
	push ebp

	mov ebp, [esp + 20 + 5*4] ; addrlen
	mov edi, [esp + 20 + 4*4] ; dest_addr
	mov esi, [esp + 20 + 3*4] ; flags
	mov edx, [esp + 20 + 2*4] ; len
	mov ecx, [esp + 20 + 1*4] ; buf
	mov ebx, [esp + 20 + 0*4] ; sockfd
	mov eax, 0x171
	int 0x80

	pop ebp
	pop edi
	pop esi
	pop ebx
	ret
