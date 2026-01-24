/*
 * pic.c
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */


#include "pic.h"


void kInitializePIC(void)
{
	// 마스터 PIC 초기화
	kOutPortByte(PIC_MASTER_PORT1, 0x11);
	kOutPortByte(PIC_MASTER_PORT2, PIC_IRQ_START_VECTOR);
	kOutPortByte(PIC_MASTER_PORT2, 0x04);
	kOutPortByte(PIC_MASTER_PORT2, 0x01);

	// 슬레이브 PIC 초기화
	kOutPortByte(PIC_SLAVE_PORT1, 0x11);
	kOutPortByte(PIC_SLAVE_PORT2, PIC_IRQ_START_VECTOR + 8);
	kOutPortByte(PIC_SLAVE_PORT2, 0x02);
	kOutPortByte(PIC_SLAVE_PORT2, 0x01);
}


void kMaskPICInterrupt(WORD wIrqMask)
{
	// 인터럽트 마스크 설정
	// 마스터 PIC
	kOutPortByte(PIC_MASTER_PORT2, (BYTE)wIrqMask);

	// 슬래이브 PIC
	kOutPortByte(PIC_MASTER_PORT2, (BYTE)(wIrqMask >> 8));
}


void kSendEOIToPIC(int iIRQNum)
{
	// PIC Controller에 EOI 전송
	// 마스터 PIC
	kOutPortByte(PIC_MASTER_PORT1, 0x20);

	// 슬레이브 PIC인 경우 슬래이브에도 EOI 전송
	if(8 <= iIRQNum) {
		kOutPortByte(PIC_SLAVE_PORT1, 0x20);
	}
}
