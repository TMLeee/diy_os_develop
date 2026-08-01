[BITS 64]

SECTION .text

extern main

; 코드 영역
START:
	; mode_sw.asm 는 여기로 저주소(물리 0x200000) 별칭으로 진입한다.
	; 이 아래 모든 심볼은 0xFFFFFFFF802xxxxx 로 링크되어 있으므로 심볼을
	; 건드리기 전에 고주소 별칭으로 넘어가야 한다.
	; far jmp imm16:imm32 는 64비트 타겟을 인코딩할 수 없어 레지스터 간접으로 뛴다.
	; 부트 페이지 테이블이 PML4[511]로 이 창을 이미 매핑해 둔 상태다
	mov rax, strict qword .high
	jmp rax

.high:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax

	; 스택 지정: 물리 0x600000 - 0x6FFFFF 를 direct map 별칭으로 잡는다.
	; identity 별칭을 쓰면 kInitializePaging 이 identity 를 걷어내는 순간
	; mov cr3 직후 첫 스택 접근에서 죽는다. 부트 테이블에 PML4[256]이 있다
	mov ss, ax
	mov rsp, strict qword 0xFFFF8000006FFFF8
	mov rbp, rsp
	
	; main 함수 호출
	call main

	jmp $

; 링커의 executable-stack 경고 억제
section .note.GNU-stack noalloc noexec nowrite progbits