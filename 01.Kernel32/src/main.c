/*
 * main.c
 *
 *  Created on: 2025. 12. 31.
 *      Author: Macbook_pro
 */

#include "types.h"
#include "page.h"
#include "mode_sw.h"

void kPrintString(int x, int y, const char* str);
BOOL kInitKernel64Area(void);
BOOL kCheckMemSizeOvr64MB(void);
void kCpy64KernelToMem(void);


void main(void)
{
	DWORD i;
	DWORD dwEAX, dwEBX, dwECX, dwEDX;
	char strCpuVender[13] = {0,};

	kPrintString(0, 3, "C Language Kernel Start.............[ OK ]");

	// 최소 메모리 크기 확인
	kPrintString(0, 4, "Check Minimum Memory Size...........[    ]");
	if( FALSE == kCheckMemSizeOvr64MB()) {
		kPrintString(37, 4, "Fail");
		kPrintString(0, 5, "Not Enough Memory. Need more than 64MB Memory.");
		while(1);
	}
	kPrintString(37, 4, " OK ");

	// IA-32e 모드의 커널 영역 초기화
	kPrintString(0, 5, "IA-32e Kernel Area Initialization...[    ]");
	if( FALSE == kInitKernel64Area()) {
		kPrintString(36, 5, "Fail");
		kPrintString(0, 6, "Fail to initializing Kernel Area.");
		while(1);
	}
	kPrintString(37, 5, " OK ");

	// IA-32e 모드 커널을 위한 페이지 생성
	kPrintString(0, 6, "IA-32e Page Table Initialization....[    ]");
	kInitPageTbl();
	kPrintString(37, 6, " OK ");

	// Read CPU Information
	kReadCPUID(0x00, &dwEAX, &dwEBX, &dwECX, &dwEDX);
	*(DWORD*)strCpuVender = dwEBX;
	*((DWORD*)strCpuVender + 1) = dwEDX;
	*((DWORD*)strCpuVender + 2) = dwECX;
	kPrintString(0, 7, "Processor Vender....................[            ]");
	kPrintString(37, 7, strCpuVender);

	// 64비트를 지원하는지 확인
	kReadCPUID(0x80000001, &dwEAX, &dwEBX, &dwECX, &dwEDX);
	kPrintString(0, 8, "Check CPU support 64Bit Mode........[    ]");
	if(dwEDX & (1 << 29)) {
		kPrintString(37, 8, " OK ");
	}
	else {
		kPrintString(37, 8, "Fail");
		kPrintString(0, 9, "CPU is not support 64Bit mode.");
		while(1);
	}

	// IA-32a 모드 전환
	kPrintString(0, 9, "Copy Kernel Code to Memory..........[    ]");
	kCpy64KernelToMem();
	kPrintString(37, 9, " OK ");

	kPrintString(0, 10, "Switch to IA-32e Mode.");
	kJump64BitKernel();

	while(1);
}


void kPrintString(int x, int y, const char* str)
{
	CharStruct* poScreen = (CharStruct*) 0xB8000;
	int i;

	poScreen += (y * 80) + x;
	for(i = 0; str[i] != 0; ++i) {
		poScreen[i].ucChar = str[i];
	}
}


BOOL kInitKernel64Area(void)
{
	DWORD* poCurAddr;

	// 1MB 주소 지정
	poCurAddr = (DWORD*) 0x100000;

	// 6MB 까지 0x00으로 초기화
	while( (DWORD)poCurAddr < 0x600000 ) {
		*poCurAddr = 0x00;
		if(*poCurAddr != 0x00) {
			return FALSE;
		}
		++poCurAddr;
	}
	return TRUE;
}


BOOL kCheckMemSizeOvr64MB(void)
{
	DWORD* poCurAddr;

	// 1MB 주소 지정
	poCurAddr = (DWORD*) 0x100000;

	// 메모리 크기 확인
	while( (DWORD)poCurAddr < 0x4000000) {
		*poCurAddr = 0x12345678;
		if(*poCurAddr != 0x12345678) {
			return FALSE;
		}
		poCurAddr += (0x100000 / 4);
	}
	return TRUE;
}


void kCpy64KernelToMem(void)
{
	WORD wKernel32SectorCnt, wTotalKernelSectorCnt;
	DWORD *poDwSrcAddr, *poDwDenstAddr;
	int i;

	wTotalKernelSectorCnt = *((WORD*)0x7C05);
	wKernel32SectorCnt = *((WORD*)0x7C07);

	poDwSrcAddr = (DWORD*)(0x10000 + (wKernel32SectorCnt * 512));
	poDwDenstAddr = (DWORD*)0x200000;

	// 코드 복사
//	int codeSize = (512 * (wTotalKernelSectorCnt - wKernel32SectorCnt)) / 4;
	for(i=0; i<(512 * (wTotalKernelSectorCnt - wKernel32SectorCnt)) / 4; ++i) {
		*poDwDenstAddr = *poDwSrcAddr;
		++poDwDenstAddr;
		++poDwSrcAddr;
	}
}
