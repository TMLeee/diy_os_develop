[BITS 64]

SECTION .text

extern main

; 코드 영역
; 저주소 별칭으로 진입한다. 심볼은 전부 0xFFFFFFFF802xxxxx라 먼저 넘어가야 한다
START:
	mov rax, strict qword .high
	jmp rax

.high:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; 스택도 direct map 별칭으로. identity 별칭이면 identity를 걷어내는 순간 죽는다
	mov ss, ax
	mov rsp, strict qword 0xFFFF8000006FFFF8
	mov rbp, rsp
	
	call main

	jmp $

; 링커의 executable-stack 경고 억제
section .note.GNU-stack noalloc noexec nowrite progbits