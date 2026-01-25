/*
 * pit.c
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */


#include "pit.h"


void kInitializePIT(WORD wCnt, BOOL bPeriodic)
{
	// PIT 초기화
	// 모드0, 바이너리 모드
	kOutPortByte(PIT_PORT_CONTROL, PIT_COUNTER0_ONCE);

	// 주기 반복은 모드 2
	if(TRUE == bPeriodic) {
		kOutPortByte(PIT_PORT_CONTROL, PIT_COUNTER0_PERIODIC);
	}

	// 카운터 0에 LSB -> MSB 순 초기화
	kOutPortByte(PIT_PORT_CONTROL0, wCnt);
	kOutPortByte(PIT_PORT_CONTROL0, wCnt >> 8);
}


WORD kReadCounter0(void)
{
	BYTE ucCountHigh, ucCountLow;
	WORD wTmp = 0;

	// Read Counter 0
	kOutPortByte(PIT_PORT_CONTROL, PIT_COUNTER0_LATCH);

	// LSB -> MSB 순으로 읽어야함
	ucCountLow	= kInPortByte(PIT_PORT_CONTROL0);
	ucCountHigh	= kInPortByte(PIT_PORT_CONTROL0);

	wTmp = ucCountHigh;
	wTmp = (wTmp << 8) | ucCountLow;
	return wTmp;
}


void kWaitUsingDirectPIT(WORD wCnt)
{
	WORD wLastCount0;
	WORD wCurrCount0;

	// 반복모드로 초기화
	kInitializePIT(0, TRUE);

	// 대기
	wLastCount0 = kReadCounter0();
	while(1) {
		wCurrCount0 = kReadCounter0();
		if(wCnt <= ((wLastCount0 - wCurrCount0) & 0xFFFF)) {
			break;
		}
	}
}
