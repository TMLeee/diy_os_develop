/*
 * serial.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_SERIAL_H_
#define __02_KERNEL64_SRC_SERIAL_H_

#include "types.h"

// COM1 베이스 포트
#define SERIAL_PORT_COM1			0x3F8

// COM1 기준 레지스터 오프셋
#define SERIAL_PORT_DATA			(SERIAL_PORT_COM1 + 0)	// DLAB=0: RBR/THR, DLAB=1: Divisor Low
#define SERIAL_PORT_INT_ENABLE		(SERIAL_PORT_COM1 + 1)	// DLAB=0: IER,     DLAB=1: Divisor High
#define SERIAL_PORT_FIFO_CONTROL	(SERIAL_PORT_COM1 + 2)	// FCR
#define SERIAL_PORT_LINE_CONTROL	(SERIAL_PORT_COM1 + 3)	// LCR
#define SERIAL_PORT_MODEM_CONTROL	(SERIAL_PORT_COM1 + 4)	// MCR
#define SERIAL_PORT_LINE_STATUS		(SERIAL_PORT_COM1 + 5)	// LSR

// Baud Rate Divisor (115200 / 1 = 115200 baud)
#define SERIAL_BAUD_DIVISOR			1

// LCR
#define SERIAL_LCR_8N1				0x03	// 8bit, No Parity, 1 Stop Bit
#define SERIAL_LCR_DLAB				0x80	// Divisor Latch Access Bit

// IER: 인터럽트 전부 비활성화
// IRQ3(벡터 35)/IRQ4(벡터 36)용 게이트는 descriptor.c에 이미 등록되어 있으나
// 범용 핸들러(kCommonInterruptHandler)로만 연결되어 있다.
// 시리얼을 인터럽트 구동으로 바꾸려면 전용 핸들러부터 만들어야 하므로,
// 그때까지는 IER=0으로 두어 폴링 전용임을 보장한다.
#define SERIAL_IER_DISABLE_ALL		0x00

// FCR: FIFO Enable + Rx/Tx FIFO Clear + 14byte Trigger
#define SERIAL_FCR_ENABLE_CLEAR		0xC7

// MCR: DTR, RTS, OUT2 활성화
#define SERIAL_MCR_DTR_RTS_OUT2		0x0B

// LSR
#define SERIAL_LSR_THR_EMPTY		0x20	// bit5, 송신 홀딩 레지스터 비어있음

// THR이 빌 때 까지 대기하는 최대 횟수
// UART가 없거나 응답하지 않는 환경에서도 부팅이 멈추지 않도록 반드시 상한을 둔다
#define SERIAL_MAX_SPIN_CNT			100000


void kInitializeSerial(void);
void kSerialPutChar(char c);
void kSerialPutString(const char* pcBuf);


#endif /* 02_KERNEL64_SRC_SERIAL_H_ */
