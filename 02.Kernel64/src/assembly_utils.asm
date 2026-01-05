[BITS 64]

SECTION .text

; C언어와 연결
global kInPortByte, kOutPortByte, kLoadGDTR, kLoadTR, kLoadIDTR

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
