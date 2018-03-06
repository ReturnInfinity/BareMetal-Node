; nasm test.asm -o test.app

[BITS 64]
[ORG 0xFFFF800000000000]

start:
	mov rdi, 0xb8000
	mov al, 0x01
	stosb
	ret
