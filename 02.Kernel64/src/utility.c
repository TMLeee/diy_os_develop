/*
 * utility.c
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */

#include "utility.h"
#include "memmap.h"
#include "assembly_utils.h"

static QWORD g_qwTotalRAMSize = 0;
volatile QWORD g_qwTickCount = 0;

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


int kStrLen(const char* str)
{
	int i;

	for(i=0; ; ++i) {
		if('\0' == str[i]) {
			break;
		}
	}

	return i;
}


// E820에서 얻은 사용 가능 용량을 MB로 환산. 예전에는 4MB 간격으로 값을 써 보고
// 읽히는지 확인하는 파괴적 프로빙을 했는데, MMIO/ACPI 구멍을 구분할 수 없었다
void kCheckTotalRAMSize(void)
{
	g_qwTotalRAMSize = kGetUsableMemorySize() / 0x100000;
}


QWORD kGetTotalRAMSize(void)
{
	return g_qwTotalRAMSize;
}


void kReverseString(char* str)
{
	int iLength;
	int i;
	char cTmp;

	iLength = kStrLen(str);
	for(i=0; i<(iLength/2); ++i) {
		cTmp = str[i];
		str[i] = str[iLength - 1 - i];
		str[iLength - 1 - i] = cTmp;
	}
}


long kAToI(const char* str, int iRadix)
{
	long lReturn;

	switch(iRadix) {
		case 16:
			lReturn = kHexStringToQword(str);
			break;

		case 10:
		default:
			lReturn = kDecimalStringToLong(str);
			break;
	}

	return lReturn;
}


QWORD kHexStringToQword(const char* str)
{
	QWORD qwValue = 0;
	int i;

	for(i=0; (str[i] != '\0'); ++i) {
		qwValue *= 16;
		if(('A' <= str[i]) && (str[i] <= 'Z')) {
			qwValue += (str[i] - 'A') + 10;
		}
		else if(('a' <= str[i]) && (str[i] <= 'z')) {
			qwValue += (str[i] - 'a') + 10;
		}
		else {
			qwValue += str[i] - '0';
		}
	}

	return qwValue;
}


long kDecimalStringToLong(const char* str)
{
	long lValue = 0;
	int i;

	// 음수인 경우
	if('-' == str[0]) {
		i = 1;
	}
	else{
		i = 0;
	}

	// 숫자로 변환
	for(; str[i] != '\0'; ++i) {
		lValue *= 10;
		lValue += str[i] - '0';
	}

	// 음수인 경우
	if('-' == str[0]) {
		lValue = -lValue;
	}
	return lValue;
}


int kIToA(long lValue, char* str, int iRadix)
{
	int iReturn;

	switch(iRadix) {
		case 16:
			iReturn = kHexToString(lValue, str);
			break;

		case 10:
		default:
			iReturn = kDecimalToString(lValue, str);
			break;
	}

	return iReturn;
}


int kHexToString(QWORD qwValue, char* str)
{
	QWORD i;
	QWORD qwCurValue;

	// 값이 0인 경우
	if(0 == qwValue) {
		str[0] = '0';
		str[1] = '\0';
		return 1;
	}

	// 숫자를 문자로 변환
	for(i=0; 0<qwValue; ++i) {
		qwCurValue = qwValue % 16;
		if(9 < qwCurValue) {
			str[i] = 'A' + (qwCurValue - 10);
		}
		else {
			str[i] = '0' + qwCurValue;
		}
		qwValue = qwValue / 16;
	}
	str[i] = '\0';

	// 순서를 뒤집어줌
	kReverseString(str);
	return i;
}


int kDecimalToString(long lValue, char* str)
{
	long i;

	// 값이 0인 경우
	if(0 == lValue) {
		str[0] = '0';
		str[1] = '\0';
		return 1;
	}

	// 음수인 경우
	if(lValue < 0) {
		i = 1;
		str[0] = '-';
		lValue = -lValue;
	}
	else{
		i = 0;
	}

	// 숫자를 문자로 변환
	for(; 0<lValue; ++i) {
		str[i] = '0' + (lValue % 10);
		lValue = lValue / 10;
	}
	str[i] = '\0';

	// 순서를 뒤집어줌
	if('-' == str[0]) {
		kReverseString(&(str[1]));
	}
	else {
		kReverseString(str);
	}

	return i;
}


int kSPrintf(char* str, const char* format, ...)
{
	va_list ap;
	int iReturn;

	// 가변 인자 추출 후 kVSPrintf 호출
	va_start(ap, format);
	iReturn = kVSPrintf(str, format, ap);
	va_end(ap);

	return iReturn;
}


// 0으로 채운 고정폭 16진 문자열. kVSPrintf에 폭 지정이 없어서 필요하다
void kToHexString(QWORD qwValue, char* pcBuff, int iDigits)
{
	int i;
	BYTE ucNibble;

	for(i=0; i<iDigits; ++i) {
		ucNibble = (BYTE)((qwValue >> ((iDigits - 1 - i) * 4)) & 0x0F);
		pcBuff[i] = (9 < ucNibble) ? ('A' + ucNibble - 10) : ('0' + ucNibble);
	}
	pcBuff[iDigits] = '\0';
}


// 부호 없는 64비트 10진 변환. kVSPrintf의 %d는 int로 잘리므로 필요하다
int kUIToDecString(QWORD qwValue, char* pcBuff)
{
	char vcTmp[24];
	int iLen = 0;
	int i;

	if(0 == qwValue) {
		pcBuff[0] = '0';
		pcBuff[1] = '\0';
		return 1;
	}

	while(0 < qwValue) {
		vcTmp[iLen] = '0' + (char)(qwValue % 10);
		qwValue /= 10;
		++iLen;
	}
	for(i=0; i<iLen; ++i) {
		pcBuff[i] = vcTmp[iLen - 1 - i];
	}
	pcBuff[iLen] = '\0';
	return iLen;
}


int kVSPrintf(char* str, const char* format, va_list ap)
{
	QWORD i;
	int iBuffIdx = 0;
	int iFormatLength, iCpyLength;
	char* poCpyStr;
	QWORD qwValue;
	int iValue;

	// Format 문자열 처리
	iFormatLength = kStrLen(format);
	for(i=0; i<iFormatLength; ++i) {
		// % 문자 처리
		if('%' == format[i]) {
			++i;
			switch(format[i]) {
				case 's':
					poCpyStr = (char*)(va_arg(ap, char*));
					iCpyLength = kStrLen(poCpyStr);
					kMemCpy(str + iBuffIdx, poCpyStr, iCpyLength);
					iBuffIdx += iCpyLength;
					break;

				case 'c':
					str[iBuffIdx] = (char)(va_arg(ap, int));
					++iBuffIdx;
					break;

				case 'd':
				case 'i':
					iValue = (int)(va_arg(ap, int));
					iBuffIdx += kIToA(iValue, str + iBuffIdx, 10);
					break;

				case 'x':
				case 'X':
					qwValue = (DWORD)(va_arg(ap, DWORD)) & 0xFFFFFFFF;
					iBuffIdx += kIToA(qwValue, str + iBuffIdx, 16);
					break;

				case 'q':
				case 'Q':
				case 'p':
					qwValue = (QWORD)(va_arg(ap, QWORD));
					iBuffIdx += kIToA(qwValue, str + iBuffIdx, 16);
					break;

				default:
					str[iBuffIdx] = format[i];
					++iBuffIdx;
					break;
			}
		}
		else {
			str[iBuffIdx] = format[i];
			++iBuffIdx;
		}
	}

	str[iBuffIdx] = '\0';
	return iBuffIdx;
}


QWORD kGetTickCnt(void)
{
	return g_qwTickCount;
}