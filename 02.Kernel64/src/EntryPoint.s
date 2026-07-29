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

; NASM은 .note.GNU-stack 섹션을 자동으로 넣지 않아 링커가 경고를 낸다.
; 플랫 바이너리로 objcopy되는 커널이라 실행 스택 표시 자체는 의미가 없지만,
; 빌드 로그를 조용하게 유지해야 새로 생기는 경고를 알아챌 수 있다.
section .note.GNU-stack noalloc noexec nowrite progbits