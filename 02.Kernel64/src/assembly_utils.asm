[BITS 64]

SECTION .text

; C언어와 연결
global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR
global kEnableInterrupt, kDisableInterrupt, kReadRFLAGS
global kReadTSC
global kSwitchContext

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
