/*
 * serial.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  COM1(0x3F8) 폴링 전용 시리얼 출력
 */

#include "types.h"
#include "serial.h"
#include "assembly_utils.h"


// THR이 빌 때 까지 대기. 타임아웃이면 FALSE
static BOOL kWaitForSerialTHREmpty(void)
{
	int i;

	for(i=0; i<SERIAL_MAX_SPIN_CNT; ++i) {
		if(kInPortByte(SERIAL_PORT_LINE_STATUS) & SERIAL_LSR_THR_EMPTY) {
			return TRUE;
		}
	}
	return FALSE;
}


void kInitializeSerial(void)
{
	// 인터럽트 비활성화 (폴링 전용)
	kOutPortByte(SERIAL_PORT_INT_ENABLE, SERIAL_IER_DISABLE_ALL);

	// DLAB 설정 후 Divisor 기록 (115200 / 1 = 115200 baud)
	kOutPortByte(SERIAL_PORT_LINE_CONTROL, SERIAL_LCR_DLAB);
	kOutPortByte(SERIAL_PORT_DATA, SERIAL_BAUD_DIVISOR & 0xFF);
	kOutPortByte(SERIAL_PORT_INT_ENABLE, (SERIAL_BAUD_DIVISOR >> 8) & 0xFF);

	// DLAB 해제, 8bit / No Parity / 1 Stop Bit
	kOutPortByte(SERIAL_PORT_LINE_CONTROL, SERIAL_LCR_8N1);

	// FIFO 활성화 및 Rx/Tx FIFO 초기화
	kOutPortByte(SERIAL_PORT_FIFO_CONTROL, SERIAL_FCR_ENABLE_CLEAR);

	// DTR, RTS, OUT2 활성화
	kOutPortByte(SERIAL_PORT_MODEM_CONTROL, SERIAL_MCR_DTR_RTS_OUT2);

	// DLAB 조작 중 0x3F9를 Divisor High로 썼으므로 IER=0 재확정
	kOutPortByte(SERIAL_PORT_INT_ENABLE, SERIAL_IER_DISABLE_ALL);
}


void kSerialPutChar(char c)
{
	// '\n'은 "\r\n"으로 확장. '\r' 실패 시 반쪽 개행을 남기지 않고 통째로 버린다
	if('\n' == c) {
		if(FALSE == kWaitForSerialTHREmpty()) {
			return;
		}
		kOutPortByte(SERIAL_PORT_DATA, '\r');
	}

	if(FALSE == kWaitForSerialTHREmpty()) {
		return;
	}
	kOutPortByte(SERIAL_PORT_DATA, (BYTE)c);
}


void kSerialPutString(const char* pcBuf)
{
	int i;

	if(NULL == pcBuf) {
		return;
	}

	for(i=0; '\0' != pcBuf[i]; ++i) {
		kSerialPutChar(pcBuf[i]);
	}
}
