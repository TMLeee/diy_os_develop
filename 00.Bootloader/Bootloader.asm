[ORG 0x00]				; 코드의 시작 어드레스 - 0x00
[BITS 16]				; 이 아래부터 16비트 코드


SECTION .text
; 여기에서 부터 코드 작성을 시작함

; 세그먼트 초기화
jmp 0x07C0:START

; -----------------부트로더 크기 정의-----------------
TOTAL_SECTOR_SIZE:		dw	0x02

KERNEL32_SECTOR_SIZE:	dw	0x02


; -----------------코드 영역-----------------
START:
	; 초기화
	mov ax, 0x07C0
	mov ds, ax
	mov ax, 0xB800
	mov es, ax

	; 스텍 생성. 영역: 0x0000 - 0xFFFF
	mov ax, 0x0000
	mov ss, ax
	mov sp, 0xFFFE
	mov bp, 0xFFFE

	; 화면 초기화
	mov si, 0

; -----------------화면 초기화 루프-----------------
.SCREEN_CLEAR_LOOP:
	mov byte [ es: si], 0
	mov byte [ es: si + 1 ], 0x07
	add si, 2
	cmp si, 80 * 25 * 2
	jl .SCREEN_CLEAR_LOOP

	; 텍스트 출력 루프
	; PRINT_MESSAGE(x, y, msg)
	push BOOT_MSG
	push 0
	push 0
	call PRINT_MESSAGE
	add sp, 6

	; 디스크 로딩 메세지 츨력
	push IMG_LOAD_MSG
	push 1
	push 0
	call PRINT_MESSAGE
	add sp, 6

; -----------------OS 이미지를 디스크에서 로딩-----------------

	; 디스크 로딩을 위한 리셋
RESET_DISK:
	; BIOS Reset 함수 호출
	; 서비스 번호: 0 (Floppy Disk)
	mov ax, 0
	mov dl, 0
	int 0x13
	; 에러 발생시 에러 핸들러 호출
	jc ERR_HANDLER_DISK

	; 디스크에서 섹터를 읽음
	mov si, 0x1000
	mov es, si
	mov bx, 0x0000
	mov di, word [ TOTAL_SECTOR_SIZE ]

	; 디스크 읽기
READ_DATA:
	cmp di, 0
	je READ_END
	sub di, 0x1

	; BIOS Read 함수 호출
	mov ah, 0x02
	mov al, 0x1
	mov ch, byte [ TRACK_NUMBER ]
	mov cl, byte [ SECTOR_NUMBER ]
	mov dh, byte [ HEAD_NUMBER ]
	mov dl, 0x00
	int 0x13
	jc ERR_HANDLER_DISK

	; 복사할 주소, 트랙, 헤드, 섹터 값 계산
	add si, 0x0020
	mov es, si

	; 섹터번호를 증가시키고 마지막 섹터(18)까지 읽었는지 확인
	mov al, byte [ SECTOR_NUMBER ]
	add al, 0x01
	mov byte [SECTOR_NUMBER ], al
	cmp al, 19
	jl READ_DATA

	; 마지막 섹터까지 읽은 경우 헤드를 토글하고 섹터를 1로 초기화
	xor byte [ HEAD_NUMBER ], 0x01
	mov byte [ SECTOR_NUMBER ], 0x01

	; 헤드가 1에서 0으로 바뀐 경우 헤드를 모두 읽은 것 임으로 트랙번호를 1 증가
	cmp byte [ HEAD_NUMBER ], 0x00
	jne READ_DATA

	; 트랙을 1 증가시키고 섹터 읽기로 이동
	add byte [ TRACK_NUMBER ], 0x01
	jmp READ_DATA
READ_END:

	; OS 이미지 로드가 완료되었음으로 메세지를 출력
	push IMG_LOAD_CMPL_MSG
	push 1
	push 20
	call PRINT_MESSAGE
	add sp, 6

	; 로드한 OS 이미지 실행
	jmp 0x1000:0x0000



; -----------------함수 코드 영역-----------------
; 디스크 에러 핸들러
ERR_HANDLER_DISK:
	push IMG_LOAD_FAIL_MSG
	push 1
	push 20
	call PRINT_MESSAGE

	jmp $


; Print 함수
PRINT_MESSAGE:
	push bp
	mov bp, sp

	push es
	push si
	push di
	push ax
	push cx
	push dx

	; ES 세그먼트에 비디오 모드 어드레스 설정
	mov ax, 0xB800
	mov es, ax

	; X, Y 좌표 계산
	mov ax, word [ bp + 6 ]
	mov si, 160
	mul si
	mov di, ax

	mov ax, word [ bp + 4 ]
	mov si, 2
	mul si
	add di, ax

	mov si, word [ bp + 8 ]

.MESSAGE_LOOP:
	mov cl, byte [ si ]
	cmp cl, 0
	je .MESSAGE_END

	mov byte [ es: di ], cl
	add si, 1
	add di, 2
	jmp .MESSAGE_LOOP
.MESSAGE_END:
	pop dx
	pop cx
	pop ax
	pop di
	pop si
	pop es
	pop bp
	ret



; -----------------데이터 영역-----------------
; 출력한 메세지 문자열
BOOT_MSG:			db 'TM-OS1 Bootloader started!', 0
IMG_LOAD_MSG:		db 'Loading OS Image...', 0
IMG_LOAD_CMPL_MSG:	db 'Complete!', 0
IMG_LOAD_FAIL_MSG:	db 'Fail', 0

SECTOR_NUMBER:		db 0x02
HEAD_NUMBER:		db 0x00
TRACK_NUMBER:		db 0x00

; 510 번지까지 0x00으로 채움
; $: 현재 라인의 주소 값
; $$: 현재 섹션(.text)의 시작 주소
; $ - $$: 현재 섹션을 기준으로 하는 오프셋
; db 0x00: 1바이트를 선언하고 0x00 기록
; times 510: 510까지 반복
times 510 - ( $ - $$ )	db	0x00

; 마지막 2바이트는 0x55, 0xAA로 채워 부트 가능함을 알려줌
db 0x55
db 0xAA
