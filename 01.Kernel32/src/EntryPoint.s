[ORG 0x00]
[BITS 16]

SECTION .text

;----------------------- 코드 영역 -----------------------
START:
	; 보호 모드 엔트리 포인트 주소(0x000)를 세그먼트 레지스터 값으로 변환
	mov ax, 0x1000
	mov ds, ax
	mov es, ax
	
	; A20 게이트 활성화
	mov ax, 0x2401
	int 0x15
	
	jc .ERR_A20_GATE
	jmp .SUCC_A20_GATE
	
.ERR_A20_GATE:
	; 에러 발생시 시스템 컨트롤 포트로 전환 시도
	in al, 0x92
	or al, 0x02
	and al, 0xFE
	out 0x92, al
	
.SUCC_A20_GATE:	
	; 인터럽트가 발생하지 못하도록 함
	cli
	
	; GDT 테이블 로드
	lgdt [ GDTR ]
	
	; 보호모드 진입
	mov eax, 0x4000003B
	mov cr0, eax
	
	; 커널 코드 세그먼트를 0x00 기준으로 하도록 하고 EIP 값 재설정
	jmp dword 0x18: ( PROTECTED_MODE - $$ + 0x10000 )
	
;----------------------- 보호 모드 진입 -----------------------
[BITS 32]
PROTECTED_MODE:
	mov ax, 0x20	; 보호 모드 커널용 데이터 세그먼트 디스크립터를 AX에 저장
	mov ds, ax		; DS 세그먼트 설렉터에 설정
	mov es, ax		; ES 세그먼트 설렉터에 설정
	mov fs, ax		; FS 세그먼트 설렉터에 설정
	mov gs, ax		; GS 세그먼트 설렉터에 설정
	
	; 스택 생성
	mov ss, ax
	mov esp, 0xFFFE
	mov ebp, 0xFFFE
	
	; 보호 모드 전환 메시지 출력
	push ( SWITCH_SUCC_MSG - $$ + 0x10000 )
	push 2
	push 0
	call PRINT_MESSAGE
	add esp, 12
	
	; 현재 위치에서 대기
	; 추후 64비트 전한 커널
    jmp dword 0x18: 0x10200 ; main() 함수 호출
	
	
;----------------------- 함수 영역 -----------------------
; Print 함수
PRINT_MESSAGE:
	push ebp
	mov ebp, esp

	push esi
	push edi
	push eax
	push ecx
	push edx

	; X, Y 좌표 계산
	mov eax, dword [ ebp + 12 ]
	mov esi, 160
	mul esi
	mov edi, eax

	mov eax, dword [ ebp + 8 ]
	mov esi, 2
	mul esi
	add edi, eax

	mov esi, dword [ ebp + 16 ]

.MESSAGE_LOOP:
	mov cl, byte [ esi ]
	cmp cl, 0
	je .MESSAGE_END

	mov byte [ edi + 0xB8000 ], cl
	add esi, 1
	add edi, 2
	jmp .MESSAGE_LOOP
	
.MESSAGE_END:
	pop edx
	pop ecx
	pop eax
	pop edi
	pop esi
	pop ebp
	ret
	
	
;----------------------- 데이터 영역 -----------------------
align 8, db 0

dw 0x0000

; GDTR 자료구조 정의
GDTR:
	dw GDTEND - GDT - 1
	dd ( GDT - $$ + 0x10000 )
	
; GDT 태이블 정의
GDT:
	; 초기화를 위한 디스크립터
	NULL_DESC:
		dw 0x0000
		dw 0x0000
		db 0x00
		db 0x00
		db 0x00
		db 0x00
		
	; IA-32e 모드용 세그먼트 디스크립터
	IA_32E_MODE_DESC:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x9A
		db 0xAF
		db 0x00
		
	; IA-32e 모드용 데이터 세그먼트 디스크립터
	IA_32E_MODE_DATA_DESC:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x92
		db 0xAF
		db 0x00
		
	; 보호 모드 커널용 코드 세그먼트 디스크립터
	CODE_DESC:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x9A
		db 0xCF
		db 0x00
		
	; 보호 모드 커널용 데이터 세그먼트 디스크립터
	DATA_DESC:
		dw 0xFFFF
		dw 0x0000
		db 0x00
		db 0x92
		db 0xCF
		db 0x00
GDTEND:


SWITCH_SUCC_MSG: db 'Switch To Protected Mode Success!', 0

; 512바이트 맞추기 위해 0으로 채움
times 512 - ( $ - $$ )  db  0x00
