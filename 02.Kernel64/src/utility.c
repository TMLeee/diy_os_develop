/*
 * utility.c
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */

#include "utility.h"
#include "assembly_utils.h"

void kMemSet(void* poDes, BYTE ucData, int iSize)
{
	for(int i=0; i<iSize; ++i) {
		((char*)poDes)[i] = ucData;
	}
}


int kMemCpy(void* poDes, const void *poSrc, int iSize)
{
	for(int i=0; i<iSize; ++i) {
		((char*)poDes)[i] = ((char*)poSrc)[i];
	}
	return iSize;
}


int kMemCmp(const void* poDes, const void* poSrc, int iSize)
{
	char cTmp;
	for(int i=0; i<iSize; ++i) {
		cTmp = ((char*)poDes)[i] - ((char*)poSrc)[i];
		if(0 != cTmp) {
			return (int)cTmp;
		}
	}
	return 0;
}


BOOL kSetInterruptFlag(BOOL bEnINT)
{
	QWORD qwRFLAGS;

	qwRFLAGS = kReadRFLAGS();
	if(TRUE == bEnINT) {
		kEnableInterrupt();
	}
	else {
		kDisableInterrupt();
	}

	if(qwRFLAGS & 0x0200) {
		return TRUE;
	}
	return FALSE;
}
