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

// IER: 인터럽트 전부 비활성화 (전용 ISR이 없으므로 폴링 전용)
#define SERIAL_IER_DISABLE_ALL		0x00

// FCR: FIFO Enable + Rx/Tx FIFO Clear + 14byte Trigger
#define SERIAL_FCR_ENABLE_CLEAR		0xC7

// MCR: DTR, RTS, OUT2 활성화
#define SERIAL_MCR_DTR_RTS_OUT2		0x0B

// LSR
#define SERIAL_LSR_THR_EMPTY		0x20	// bit5, 송신 홀딩 레지스터 비어있음

// THR 대기 상한. UART가 없어도 부팅이 멈추지 않도록
#define SERIAL_MAX_SPIN_CNT			100000


void kInitializeSerial(void);
void kSerialPutChar(char c);
void kSerialPutString(const char* pcBuf);


#endif /* 02_KERNEL64_SRC_SERIAL_H_ */
