/*
 * pic.h
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_PIC_H_
#define __02_KERNEL64_SRC_PIC_H_

#include "types.h"

// I/O Port
#define PIC_MASTER_PORT1	0x20
#define PIC_MASTER_PORT2	0x21
#define PIC_SLAVE_PORT1		0xA0
#define PIC_SLAVE_PORT2		0xA1

// IDT 테이블에서 Interrupt Vector 시작 위치
#define PIC_IRQ_START_VECTOR	0x20


// 함수
void kInitializePIC(void);
void kMaskPICInterrupt(WORD wIrqMask);
void kSendEOIToPIC(int iIRQNum);


#endif /* 02_KERNEL64_SRC_PIC_H_ */
