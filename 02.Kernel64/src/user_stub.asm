[BITS 64]

SECTION .text

global kUserStubStart, kUserStubEnd
global kUserBadStubStart, kUserBadStubEnd
global kUserDemandStubStart, kUserDemandStubEnd
global kUserForkStubStart, kUserForkStubEnd

; 커널 .text에 링크되지만 유저 페이지로 복사돼 돌아간다. 그래서 위치 독립이어야
; 하고, 문자열을 RIP 상대로 잡는다
kUserStubStart:
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .msg]
	mov rdx, .msgend - .msg
	int 0x80

	mov rax, 39
	int 0x80

	; 유저에서 끝낼 방법이 아직 없다. 셸이 걷어낸다
.spin:
	jmp .spin

.msg:	db "ring3 syscall ok", 10
.msgend:
kUserStubEnd:


; U/S 경계가 서 있으면 이 태스크만 죽어야 한다
kUserBadStubStart:
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .msg]
	mov rdx, .msgend - .msg
	int 0x80

	mov rax, 0xFFFFFFFF80200000
	mov rax, [rax]

.spin:
	jmp .spin

.msg:	db "ring3 about to touch kernel", 10
.msgend:
kUserBadStubEnd:


; VMA만 있고 프레임은 없다. 건드릴 때마다 커널이 붙여야 한다
kUserDemandStubStart:
	; 유저 스택도 없다. 이 push가 첫 폴트를 낸다
	push rax
	pop rax

	mov rbx, 0x500000
	mov rcx, 16
.touch:
	mov [rbx], rcx
	add rbx, 0x1000
	dec rcx
	jnz .touch

	; 폴트마다 새 프레임이 제대로 붙었는지 되읽어 확인
	mov rbx, 0x500000
	mov rcx, 16
.verify:
	cmp [rbx], rcx
	jne .bad
	add rbx, 0x1000
	dec rcx
	jnz .verify

	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .okmsg]
	mov rdx, .okend - .okmsg
	int 0x80
	jmp .spin

.bad:
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .badmsg]
	mov rdx, .badend - .badmsg
	int 0x80

.spin:
	jmp .spin

.okmsg:	db "demand paging ok", 10
.okend:
.badmsg:	db "demand paging MISMATCH", 10
.badend:
kUserDemandStubEnd:


; 자식이 공유 페이지에 쓴다. COW가 돌면 부모 값은 그대로여야 한다
kUserForkStubStart:
	mov rbx, 0x500000
	mov qword [rbx], 0x1111

	mov rax, 57				; SYS_FORK
	int 0x80
	test rax, rax
	jz .child

; ---- 부모 ----
	mov rax, 201
	int 0x80
	mov r12, rax
	add r12, 200
.wait:
	mov rax, 201
	int 0x80
	cmp rax, r12
	jb .wait

	mov rbx, 0x500000
	cmp qword [rbx], 0x1111
	jne .clobbered

	; 참조가 하나뿐이므로 복사 없이 권한만 돌려받아야 한다(reuse 경로)
	mov qword [rbx], 0x3333
	cmp qword [rbx], 0x3333
	jne .clobbered

	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .okmsg]
	mov rdx, .okend - .okmsg
	int 0x80
	jmp .spin

.clobbered:
	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .badmsg]
	mov rdx, .badend - .badmsg
	int 0x80
	jmp .spin

; ---- 자식 ----
.child:
	; PTE가 RO로 강등돼 있으므로 여기서 COW 폴트가 난다
	mov rbx, 0x500000
	mov qword [rbx], 0x2222

	cmp qword [rbx], 0x2222
	jne .spin

	mov rax, 1
	mov rdi, 1
	lea rsi, [rel .childmsg]
	mov rdx, .childend - .childmsg
	int 0x80

.spin:
	jmp .spin

.okmsg:		db "parent data intact after child write", 10
.okend:
.badmsg:	db "parent data CLOBBERED", 10
.badend:
.childmsg:	db "child wrote its own copy", 10
.childend:
kUserForkStubEnd:


; 링커의 executable-stack 경고 억제
section .note.GNU-stack noalloc noexec nowrite progbits
