/*
 * utility.h
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_UTILITY_H_
#define __02_KERNEL64_SRC_UTILITY_H_

#include <stdarg.h>
#include "types.h"


void kMemSet(void* poDes, BYTE ucData, int iSize);
int kMemCpy(void* poDes, const void *poSrc, int iSize);
int kMemCmp(const void* poDes, const void* poSrc, int iSize);
BOOL kSetInterruptFlag(BOOL bEnINT);
int kStrLen(const char* str);
void kCheckTotalRAMSize(void);
QWORD kGetTotalRAMSize(void);
void kReverseString(char* str);
long kAToI(const char* str, int iRadix);
QWORD kHexStringToQword(const char* str);
long kDecimalStringToLong(const char* str);
int kIToA(long lValue, char* str, int iRadix);
int kHexToString(QWORD qwValue, char* str);
int kDecimalToString(long lValue, char* str);
int kSPrintf(char* str, const char* format, ...);
int kVSPrintf(char* str, const char* format, va_list ap);



#endif /* 02_KERNEL64_SRC_UTILITY_H_ */
