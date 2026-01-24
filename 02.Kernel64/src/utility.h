/*
 * utility.h
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_UTILITY_H_
#define __02_KERNEL64_SRC_UTILITY_H_

#include "types.h"

void kMemSet(void* poDes, BYTE ucData, int iSize);
int kMemCpy(void* poDes, const void *poSrc, int iSize);
int kMemCmp(const void* poDes, const void* poSrc, int iSize);
BOOL kSetInterruptFlag(BOOL bEnINT);



#endif /* 02_KERNEL64_SRC_UTILITY_H_ */
