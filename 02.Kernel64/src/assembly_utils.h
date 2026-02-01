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


#endif /* 02_KERNEL64_SRC_ASSEMBLY_UTILS_H_ */
