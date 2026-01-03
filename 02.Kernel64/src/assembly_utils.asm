[BITS 64]

SECTION .text

; C언어와 연결
global kInPortByte, kOutPortByte

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
