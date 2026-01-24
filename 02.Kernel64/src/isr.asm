[BITS 64]

SECTION .text

; Import
extern kCommonExceptionHandler, kCommonInterruptHandler, kKeyboardHandler

; Export
; 예외 처리를 위한 ISR
global kISRDivideError
global kISRDebug
global kISRNMI
global kISRBreakPoint
global kISROverFlow
global kISRBoundRangeExceeded
global kISRInvalidOpcode
global kISRDeviceNotAvailable
global kISRDoubleFault
global kISRCoprocessorSegmentOverrun
global kISRInvalidTSS
global kISRSegmentNotPresent
global kISRStackSegmentFault
global kISRGeneralProtection
global kISRPageFault
global kISR15
global kISRFPUError
global kISRAlignmentCheck
global kISRMachineCheck
global kISRSIMDError
global kISRETCException


; 인터럽트 처리를 위한 ISR
global kISRTimer
global kISRKeyboard
global kISRSlavePIC
global kISRSerial2
global kISRSerial1
global kISRParallel2
global kISRFloppy
global kISRParallel1
global kISRRTC
global kISRReserved
global kISRNotUsed1
global kISRNotUsed2
global kISRMouse
global kISRCoprocessor
global kISRHDD1
global kISRHDD2
global kISRETCInterrupt

; 콘텍스트 저장 메크로
%macro KSAVECONTEXT 0
	; RBP 부터 GS까지 스택에 삽입
	push rbp
	mov rbp, rsp
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

	; DS, ES는 스택에 바로 삽입할 수 없음으로 rax를 통해서 삽입
	mov ax, ds
	push rax
	mov ax, es
	push rax
	push fs
	push gs

	; 세그먼트 설렉터 교페
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov gs, ax
	mov fs, ax
%endmacro

; 콘텍스트 복원 메크로
%macro KLOADCONTEXT 0
	; GS부터 RBP까지 복원
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

;--------------------------------------------------------------------------------
; 예외(Exception) 핸들러
; Divide Error ISR
kISRDivideError:
	KSAVECONTEXT

	mov rdi, 0
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Debug ISR
kISRDebug:
	KSAVECONTEXT

	mov rdi, 1
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; NMI ISR
kISRNMI:
	KSAVECONTEXT

	mov rdi, 2
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; BreakPoint ISR
kISRBreakPoint:
	KSAVECONTEXT

	mov rdi, 3
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Overflow ISR
kISROverFlow:
	KSAVECONTEXT

	mov rdi, 4
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Bound Range Exceeded ISR
kISRBoundRangeExceeded:
	KSAVECONTEXT

	mov rdi, 5
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Invalid Opcode ISR
kISRInvalidOpcode:
	KSAVECONTEXT

	mov rdi, 6
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Device Not Available ISR
kISRDeviceNotAvailable:
	KSAVECONTEXT

	mov rdi, 7
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Double Fault ISR
kISRDoubleFault:
	KSAVECONTEXT

	mov rdi, 8
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; Coprocessor Segment Overrun ISR
kISRCoprocessorSegmentOverrun:
	KSAVECONTEXT

	mov rdi, 9
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Invalid TSS ISR
kISRInvalidTSS:
	KSAVECONTEXT

	mov rdi, 10
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; Segment Not Present ISR
kISRSegmentNotPresent:
	KSAVECONTEXT

	mov rdi, 11
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; Stack Segment Fault ISR
kISRStackSegmentFault:
	KSAVECONTEXT

	mov rdi, 12
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; General Protection ISR
kISRGeneralProtection:
	KSAVECONTEXT

	mov rdi, 13
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; Page Fault ISR
kISRPageFault:
	KSAVECONTEXT

	mov rdi, 14
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; Reserved ISR
kISR15:
	KSAVECONTEXT

	mov rdi, 15
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; FPU Error ISR
kISRFPUError:
	KSAVECONTEXT

	mov rdi, 16
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Alignment Check ISR
kISRAlignmentCheck:
	KSAVECONTEXT

	mov rdi, 17
	mov rsi, qword [rbp+8]
	call kCommonExceptionHandler

	KLOADCONTEXT
	add rsp, 8
	iretq

; Machine Check ISR
kISRMachineCheck:
	KSAVECONTEXT

	mov rdi, 18
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; SIMD Floating Point Exception ISR
kISRSIMDError:
	KSAVECONTEXT

	mov rdi, 19
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq

; Reserved ISR (20-31)
kISRETCException:
	KSAVECONTEXT

	mov rdi, 20
	call kCommonExceptionHandler

	KLOADCONTEXT
	iretq


;--------------------------------------------------------------------------------
; 인터럽트(Interrupt) 핸들러
; Timer ISR
kISRTimer:
	KSAVECONTEXT

	mov rdi, 32
	call kCommonInterruptHandler

	KLOADCONTEXT
	iretq

; Keyboard ISR
kISRKeyboard:
	KSAVECONTEXT

	mov rdi, 33
	call kKeyboardHandler

	KLOADCONTEXT
	iretq

; Slave PIC ISR
kISRSlavePIC:
	KSAVECONTEXT

	mov rdi, 34
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Serial Port 2 ISR
kISRSerial2:
	KSAVECONTEXT

	mov rdi, 35
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Serial Port 1 ISR
kISRSerial1:
	KSAVECONTEXT

	mov rdi, 36
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

;; Parallel Port 2 ISR
kISRParallel2:
	KSAVECONTEXT

	mov rdi, 37
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Floppy Disk ISR
kISRFloppy:
	KSAVECONTEXT

	mov rdi, 38
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Parallel Port 1 ISR
kISRParallel1:
	KSAVECONTEXT

	mov rdi, 39
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; RTC ISR
kISRRTC:
	KSAVECONTEXT

	mov rdi, 40
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Reserved ISR
kISRReserved:
	KSAVECONTEXT

	mov rdi, 41
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Not Used 1
kISRNotUsed1:
		KSAVECONTEXT

	mov rdi, 42
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Not Used 2
kISRNotUsed2:
	KSAVECONTEXT

	mov rdi, 43
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Mouse ISR
kISRMouse:
	KSAVECONTEXT

	mov rdi, 44
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Coprocessor ISR
kISRCoprocessor:
	KSAVECONTEXT

	mov rdi, 45
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Hard Disk 1 ISR
kISRHDD1:
	KSAVECONTEXT

	mov rdi, 46
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; Hard Disk 2 ISR
kISRHDD2:
	KSAVECONTEXT

	mov rdi, 47
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq

; ETC ISR
kISRETCInterrupt:
	KSAVECONTEXT

	mov rdi, 48
	call kCommonInterruptHandler

	KSAVECONTEXT
	iretq
