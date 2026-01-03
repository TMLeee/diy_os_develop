[BITS 64]

SECTION .text

extern main

; 코드 영역
START:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	
	; 스택 지정: 0x600000 - 0x6FFFFF
	mov ss, ax
	mov rsp, 0x6FFFF8
	mov rbp, 0x6FFFF8
	
	; main 함수 호출
	call main
	
	jmp $