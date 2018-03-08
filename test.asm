; nasm test.asm -o test.app

[BITS 64]
[ORG 0xFFFF800000000000]

%INCLUDE "os/BareMetal-kernel/api/libBareMetal.asm"

main:					; Start of program label

	xor eax, eax			; Gather screen details
	mov eax, [0x5080]
	mov [fbuff], rax		; Address of frame buffer
	mov ax, [0x5084]
	mov [x_res], ax			; X resolution
	mov ax, [0x5086]
	mov [y_res], ax			; Y resolution
	mov al, [0x5088]
	mov [depth], al			; Color depth

	mov rsi, hello_message		; Load RSI with memory address of string
	mov rcx, 12			; Number of characters to output
	call [b_output]			; Print the string that RSI points to

	mov rdi, [fbuff]
	mov eax, 0xFFFFFF00
	stosd

	ret				; Return to OS

hello_message: db 'Node online', 10, 0

fbuff: dq 0
x_res: dw 0
y_res: dw 0
depth: db 0

; EOF
