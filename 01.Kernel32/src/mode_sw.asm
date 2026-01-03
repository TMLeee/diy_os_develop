[BITS 32]

; C 언어와 연결
global kReadCPUID, kJump64BitKernel

SECTION .text

; CPU 정보를 읽어옴
; kReadCPUID(DWORD dwEAX, DWORD *poDwEAX, DWORD *poDwEBX, DWORD *poDwECX, DWORD *poDwEDX)
kReadCPUID:
	push ebp
	mov ebp, esp
	push eax
	push ebx
	push ecx
	push edx
	push esi

	; CPU ID 명령어 실행
	mov eax, dword [ ebp + 8 ]
	cpuid

	; 파라미터 저장
	; poDwEAX
	mov esi, dword [ ebp + 12 ]
	mov dword [ esi ], eax

	; poDwEBX
	mov esi, dword [ ebp + 16 ]
	mov dword [ esi ], ebx

	; poDwECX
	mov esi, dword [ ebp + 20 ]
	mov dword [ esi ], ecx

	; poDwEDX
	mov esi, dword [ ebp + 24 ]
	mov dword [ esi ], edx

	pop esi
	pop edx
	pop ecx
	pop ebx
	pop eax
	pop ebp
	ret


; IA-32e 모드 전환 후 64비트 커널 수행
; kJump64BitKernel(void)
kJump64BitKernel:
	; CR4 레지스터의 PAE 비트를 1로 설정
	mov eax, cr4
	or eax, 0x20
	mov cr4, eax

	; CR3 레지스터에 PML4 테이블 주소 지정 및 캐시 활성화
	mov eax, 0x100000
	mov cr3, eax

	; IA32_EFER.LME를 1로 설정하여 IA-32e 활성화
	mov ecx, 0xC0000080
	rdmsr
	or eax, 0x0100
	wrmsr

	; CR0 레지스터 내 NW=0, CD=0, PG=1로 변경
	; 캐시, 페이징 기능 활성화
	mov eax, cr0
	or eax, 0xE0000000
	xor eax, 0x60000000
	mov cr0, eax

	jmp 0x08:0x200000

	jmp $
