[BITS 64]

SECTION .text

; C언어와 연결
global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR
global kEnableInterrupt, kDisableInterrupt, kReadRFLAGS
global kReadTSC
global kSwitchContext
global kReadCR0, kReadCR2, kReadCR3, kReadCR4
global kWriteCR0, kWriteCR3, kWriteCR4
global kInvlpg, kFlushTLB
global kReadCPUID
global kHlt
global kReadMSR, kWriteMSR

; 포트로부터 1바이트를 읽어옴
; BYTE kInPortByte(WORD wPort)
kInPortByte:
	push rdx

	mov rdx, rdi
	mov rax, 0

	; 포트로 부터 1바이트를 읽어옴
	in al, dx

	pop rdx
	ret


; 포트에 1바이트를 씀
; void kOutPortByte(WORD wPort, BYTE ucData)
kOutPortByte:
	push rdx
	push rax

	mov rdx, rdi
	mov rax, rsi

	; 포트에 1바이트를 씀
	out dx, al

	pop rax
	pop rdx
	ret


; GDTR 레지스터에 GDT 등록
; void kLoadGDTR(QWORD qwGDTRAddr
kLoadGDTR:
	lgdt [ rdi ]
	ret


; TR 레지스터에 TSS 등록
; void kLoadTR(WORD wTSSSegOfs)
kLoadTR:
	ltr di
	ret


; IDTR 레지스터에 IDT 등록
; void kLoadIDTR(QWORD qwIDTRAddr)
kLoadIDTR:
	lidt [ rdi ]
	ret


; 인터럽트 활성화
; void kEnableInterrupt(void)
kEnableInterrupt:
	sti
	ret

; 인터럽트 비활성화
; void kDisableInterrupt(void)
kDisableInterrupt:
	cli
	ret

; RFLAGS 레지스터 읽기
; QWORD kReadRFLAGS(void)
kReadRFLAGS:
	pushfq
	pop rax

	ret

; TSC 카운터 값을 읽고 반환
; QWORD kReadTSC(void)
kReadTSC:
	push rdx

	rdtsc

	shl rdx, 32
	or rax, rdx

	pop rdx
	ret

; QWORD kReadCR0/2/3/4(void)
kReadCR0:
	mov rax, cr0
	ret

kReadCR2:
	mov rax, cr2
	ret

kReadCR3:
	mov rax, cr3
	ret

kReadCR4:
	mov rax, cr4
	ret

; void kWriteCR0/3/4(QWORD qwValue)
kWriteCR0:
	mov cr0, rdi
	ret

kWriteCR3:
	mov cr3, rdi
	ret

kWriteCR4:
	mov cr4, rdi
	ret

; void kReadCPUID(DWORD dwEAX, DWORD* pdwEAX, DWORD* pdwEBX,
;                 DWORD* pdwECX, DWORD* pdwEDX)
kReadCPUID:
	push rbx
	push r10
	push r11

	; cpuid는 rbx/rcx/rdx를 덮으므로 출력 포인터를 먼저 옮겨 둔다
	mov r10, rdx			; pdwEBX
	mov r11, rcx			; pdwECX
	mov r9, r8				; pdwEDX (r8은 cpuid가 건드리지 않지만 통일)
	mov rax, rdi			; 요청 leaf
	mov rdi, rsi			; pdwEAX

	cpuid

	mov dword [ rdi ], eax
	mov dword [ r10 ], ebx
	mov dword [ r11 ], ecx
	mov dword [ r9 ], edx

	pop r11
	pop r10
	pop rbx
	ret

; void kInvlpg(QWORD qwVirtAddr)
kInvlpg:
	invlpg [rdi]
	ret

; void kFlushTLB(void) - CR3 재적재로 global이 아닌 모든 엔트리를 비운다
kFlushTLB:
	mov rax, cr3
	mov cr3, rax
	ret

; void kHlt(void)
kHlt:
	hlt
	ret

; void kReadMSR(DWORD dwMSR, QWORD* pqwValue)
kReadMSR:
	push rax
	push rcx
	push rdx

	mov rcx, rdi
	rdmsr					; ECX의 MSR 번호를 읽어 EDX:EAX로 반환
	shl rdx, 32
	or rax, rdx
	mov qword [ rsi ], rax

	pop rdx
	pop rcx
	pop rax
	ret

; void kWriteMSR(DWORD dwMSR, QWORD qwValue)
kWriteMSR:
	push rax
	push rcx
	push rdx

	mov rcx, rdi
	mov rax, rsi
	mov rdx, rsi
	shr rdx, 32
	wrmsr					; ECX의 MSR에 EDX:EAX를 씀

	pop rdx
	pop rcx
	pop rax
	ret

; Context를 저장하고 셀렉터를 교체하는 메크로
%macro KSAVECONTEXT 0
	push rbp
	push rax
	push rbx
	push rcx
	push rdx
	push rdi
	push rsi
	push r8
	push r9
	push r10
	push r11
	push r12
	push r13
	push r14
	push r15

	mov ax, ds
	push rax
	mov ax, es
	push rax
	push fs
	push gs
%endmacro

; Context를 복원하는 메트로
%macro KLOADCONTEXT 0
	pop gs
	pop fs
	pop rax
	mov es, ax
	pop rax
	mov ds, ax

	pop r15
	pop r14
	pop r13
	pop r12
	pop r11
	pop r10
	pop r9
	pop r8
	pop rsi
	pop rdi
	pop rdx
	pop rcx
	pop rbx
	pop rax
	pop rbp
%endmacro

; Task 전환을 위한 Context 전환
; void kSwitchContext(Context_t* poCurrContext, Context_t* poNextContext)
kSwitchContext:
	push rbp
	mov rbp, rsp

	; Current Context 주소값이 없는 경우 저장X
	pushfq
	cmp rdi, 0
	je .LoadContext
	popfq

	; Context Offset 위치 저장
	push rax

	mov ax, ss
	mov qword[rdi + (23*8)], rax

	; RBP 레지스터의 값 저장
	mov rax, rbp
	add rax, 16		; Push rbp, Return Address를 제외한 값을 복원
	mov qword[rdi + (22*8)], rax

	; RFLAGS 레지스터 저장
	pushfq
	pop rax
	mov qword[rdi + (21*8)], rax

	; CS 레지스터 저장
	mov ax, cs
	mov qword[rdi + (20*8)], rax

	; RIP 레지스터를 Return Address로 지정하여 호출한 위치로 돌아오도록 저장
	mov rax, qword[rbp + 8]
	mov qword[rdi + (19*8)], rax

	; 저장한 레지스터 복구 후 나머지 Context를 모두 저장
	pop rax
	pop rbp

	add rdi, (19*8)
	mov rsp, rdi
	sub rdi, (19*8)

	; Context 자료구조에 레지스터를 저장
	KSAVECONTEXT


; 다음 테스크의 Context를 복원함
.LoadContext:
	mov rsp, rsi

	; Context 자료구조에서 레지스터 복원
	KLOADCONTEXT
	iretq



; QWORD kDoSyscall(QWORD qwNum, QWORD a1, QWORD a2, QWORD a3)
; SysV 인자(rdi,rsi,rdx,rcx)를 시스템 콜 규약(rax,rdi,rsi,rdx)으로 옮긴다.
; 각 mov는 원본을 덮기 전에 읽으므로 임시 레지스터가 필요 없다.
; int 0x80은 인터럽트 게이트라 핸들러가 rcx/r11까지 복원해 준다
global kDoSyscall
kDoSyscall:
	mov rax, rdi
	mov rdi, rsi
	mov rsi, rdx
	mov rdx, rcx
	int 0x80
	ret

; 링커의 executable-stack 경고 억제
section .note.GNU-stack noalloc noexec nowrite progbits
