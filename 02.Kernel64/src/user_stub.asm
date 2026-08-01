[BITS 64]

SECTION .text

global kUserStubStart, kUserStubEnd
global kUserBadStubStart, kUserBadStubEnd

; ring3에서 도는 시험용 프로그램. 커널 .text 안에 링크되지만 실행은 유저
; 페이지로 복사한 뒤에 하므로 위치 독립이어야 한다 - 문자열을 RIP 상대로 잡는
; 이유다. ELF 로더가 붙는 25d-2에서 진짜 바이너리로 교체된다
kUserStubStart:
	; sys_write(1, msg, len)
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .msg]
	mov rdx, .msgend - .msg
	int 0x80

	; sys_getpid() - 반환값이 오는지도 확인한다
	mov rax, 39
	int 0x80

	; 유저에서 종료할 방법이 아직 없다. 셸이 kEndTask로 걷어낸다
.spin:
	jmp .spin

.msg:	db "ring3 syscall ok", 10
.msgend:
kUserStubEnd:


; 커널 주소를 건드리는 ring3 프로그램. U/S 경계가 서 있으면 여기서 #PF가 나고
; 이 태스크만 죽어야 한다. 커널이 같이 죽으면 경계가 없는 것이다
kUserBadStubStart:
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .msg]
	mov rdx, .msgend - .msg
	int 0x80

	; 커널 이미지의 고주소 별칭. ring3에서는 절대 닿을 수 없다
	mov rax, 0xFFFFFFFF80200000
	mov rax, [rax]

.spin:
	jmp .spin

.msg:	db "ring3 about to touch kernel", 10
.msgend:
kUserBadStubEnd:


; 링커의 executable-stack 경고 억제
section .note.GNU-stack noalloc noexec nowrite progbits
