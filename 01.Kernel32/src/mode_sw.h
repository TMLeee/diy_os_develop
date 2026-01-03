/*
 * mode_sw.h
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#ifndef __01_KERNEL32_SRC_MODE_SW_H_
#define __01_KERNEL32_SRC_MODE_SW_H_

#include "types.h"

void kReadCPUID(DWORD dwEAX, DWORD *poDwEAX, DWORD *poDwEBX, DWORD *poDwECX, DWORD *poDwEDX);
void kJump64BitKernel(void);


#endif /* 01_KERNEL32_SRC_MODE_SW_H_ */
