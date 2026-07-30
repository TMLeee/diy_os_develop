/*
 * assembly_utils.h
 *
 *  Created on: 2026. 1. 2.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_ASSEMBLY_UTILS_H_
#define __02_KERNEL64_SRC_ASSEMBLY_UTILS_H_

#include "types.h"
#include "task.h"

BYTE kInPortByte(WORD wPort);
void kOutPortByte(WORD wPort, BYTE ucData);
void kLoadGDTR(QWORD qwGDTRAddr);
void kLoadTR(WORD wTSSSegOfs);
void kLoadIDTR(QWORD qwIDTRAddr);
void kEnableInterrupt(void);
void kDisableInterrupt(void);
QWORD kReadRFLAGS(void);
QWORD kReadTSC(void);
void kSwitchContext(Context_t* poCurrContext, Context_t* poNextContext);
QWORD kReadCR0(void);
QWORD kReadCR2(void);
QWORD kReadCR3(void);
QWORD kReadCR4(void);
void kWriteCR0(QWORD qwValue);
void kWriteCR3(QWORD qwValue);
void kWriteCR4(QWORD qwValue);
void kInvlpg(QWORD qwVirtAddr);
void kReadCPUID(DWORD dwEAX, DWORD* pdwEAX, DWORD* pdwEBX,
		DWORD* pdwECX, DWORD* pdwEDX);
void kFlushTLB(void);
void kHlt(void);
void kReadMSR(DWORD dwMSR, QWORD* pqwValue);
void kWriteMSR(DWORD dwMSR, QWORD qwValue);


#endif /* 02_KERNEL64_SRC_ASSEMBLY_UTILS_H_ */
