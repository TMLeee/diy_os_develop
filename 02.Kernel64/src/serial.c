/*
 * serial.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  COM1(0x3F8) 폴링 전용 시리얼 출력.
 *  화면(VGA)은 스크롤되면 지나간 내용을 잃어버리지만 시리얼 로그는 남는다.
 *  부팅 초기 단계나 예외 발생 시점의 진단 출력을 위한 경로.
 */

#include "types.h"
#include "serial.h"
#include "assembly_utils.h"


/*
 *  송신 홀딩 레지스터(THR)가 빌 때 까지 대기
 *  UART가 응답하지 않아도 부팅이 멈추지 않도록 대기 횟수를 제한한다
 */
static BOOL kWaitForSerialTHREmpty(void)
{
	int i;

	for(i=0; i<SERIAL_MAX_SPIN_CNT; ++i) {
		// LSR(0x3F8+5)의 bit5가 1이면 송신 가능
		if(kInPortByte(SERIAL_PORT_LINE_STATUS) & SERIAL_LSR_THR_EMPTY) {
			return TRUE;
		}
	}

	// 타임아웃, 호출자는 문자를 버린다
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

	// DLAB 조작으로 IER 위치(0x3F9)를 Divisor High로 썼으므로
	// DLAB을 내린 지금 다시 IER=0을 확정한다
	kOutPortByte(SERIAL_PORT_INT_ENABLE, SERIAL_IER_DISABLE_ALL);
}


void kSerialPutChar(char c)
{
	// 터미널 정렬을 위해 '\n'은 "\r\n"으로 확장
	// '\r'을 보내지 못했다면 '\n'만 단독으로 내보내지 않고 문자 전체를 버린다.
	// (반쪽짜리 개행은 로그 정렬을 깨뜨리고, 대기 시간도 두 배로 쓰게 된다)
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
