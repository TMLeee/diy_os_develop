/*
 * descriptor.c
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */


#include "descriptor.h"
#include "utility.h"


void kInitGDTTableAndTSS(void)
{
	GDTR* poGDTR;
	GDTEntry8_t* poEntry;
	TSSSegment_t* poTSS;
	int i;

	// GDTR 설정
	poGDTR = (GDTR*) GDTR_START_ADDR;
	poEntry = (GDTEntry8_t*) (GDTR_START_ADDR + sizeof(GDTR));
	poGDTR->wLimit = GDT_TBL_SIZE - 1;
	poGDTR->qwBaseAddr = (QWORD)poEntry;

	// TSS 영역 설정
	poTSS = (TSSSegment_t*) ((QWORD)poEntry + GDT_TBL_SIZE);

	// NULL, 64Bit, Code/Data, TSS - 4개의 세그먼트 생성
	kSetGDTEntry8(&(poEntry[0]), 0, 0, 0, 0, 0);
	kSetGDTEntry8(&(poEntry[1]), 0, 0xFFFFF, GDT_FLAG_UPPER_CODE, GDT_FLAG_LOWER_KERNELCODE, GDT_TYPE_CODE);
	kSetGDTEntry8(&(poEntry[2]), 0, 0xFFFFF, GDT_FLAG_UPPER_DATA, GDT_FLAG_LOWER_KERNELDATA, GDT_TYPE_DATA);
	kSetGDTEntry16((GDTEntry16_t*) &(poEntry)[3], (QWORD)poTSS, sizeof(TSSSegment_t) - 1, GDT_FLAG_UPPER_TSS,
					GDT_FLAG_LOWER_TSS, GDT_TYPE_TSS);

	// TSS 초기화
	kInitTSSSegment(poTSS);
}


void kSetGDTEntry8(GDTEntry8_t *poEntry, DWORD dwBaseAddr, DWORD dwLimit,
				BYTE ucUppFlag, BYTE ucLowFlag, BYTE ucType)
{
	poEntry->wLowLimit = dwLimit & 0xFFFF;
	poEntry->wLowBaseAddr = dwBaseAddr & 0xFFFF;
	poEntry->ucUppBaseAddr1 = (dwBaseAddr >> 16) & 0xFF;
	poEntry->ucTypeAndLowFlag = ucLowFlag | ucType;
	poEntry->ucUppLimitAndUppFlag = ((dwLimit >> 16) & 0x0F) | ucUppFlag;
	poEntry->ucUppBaseAddr2 = (dwBaseAddr >> 24) & 0xFF;
}


void kSetGDTEntry16(GDTEntry16_t *poEntry, QWORD qwBaseAddr, DWORD dwLimit,
				BYTE ucUppFlag, BYTE ucLowFlag, BYTE ucType)
{
	poEntry->wLowLimit = dwLimit & 0xFFFF;
	poEntry->wLowBaseAddr = qwBaseAddr & 0xFFFF;
	poEntry->ucMidBaseAddr1 = (qwBaseAddr >> 16) & 0xFF;
	poEntry->ucTypeAndLowFlag = ucLowFlag | ucType;
	poEntry->ucUppLimitAndUppFlag = ((dwLimit >> 16) & 0x0F) | ucUppFlag;
	poEntry->ucMidBaseAddr2 = (qwBaseAddr >> 24) & 0xFF;
	poEntry->dwUppBaseAddr = qwBaseAddr >> 32;
	poEntry->dwReserved = 0;
}


void kInitTSSSegment(TSSSegment_t *poTSS)
{
	kMemSet(poTSS, 0, sizeof(TSSSegment_t));
	poTSS->qwIST[0] = IST_START_ADDR + IST_SIZE;

	// IO 영역 침범 방지 - TSS의 리밋보다 크게
	poTSS->wIOMapBaseAddr = 0xFFFF;
}


void kInitTDTTable(void)
{
	IDTR* poIDTR;
	IDTEntry_t* poEntry;
	int i;

	// IDTR 시작 주소
	poIDTR = (IDTR*) IDTR_START_ADDR;

	// IDTR 테이블 정보 설정
	poEntry = (IDTEntry_t*)(IDTR_START_ADDR + sizeof(IDTR));
	poIDTR->qwBaseAddr = (QWORD)poEntry;
	poIDTR->wLimit = IDT_TBL_SIZE - 1;

	// 백터 테이블에 DummyHandler를 연결함
	for(i=0; i<IDT_ENTRY_SIZE; ++i) {
		kSetIDTEntry(&(poEntry[i]), kDummyHandler, 0x08, IDT_FLAG_IST1,
					IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	}
}


void kSetIDTEntry(IDTEntry_t *poEntry, void* pvHandler, WORD wSelector,
				BYTE ucIST, BYTE ucFlag, BYTE ucType)
{
	// IDT Gate Decriptor 설정
	poEntry->wLowBaseAddr = (QWORD)pvHandler & 0xFFFF;
	poEntry->wSegSelector = wSelector;
	poEntry->ucIST = ucIST & 0x03;
	poEntry->ucTypeAndFlag = ucType | ucFlag;
	poEntry->wMidBaseAddr = (((QWORD)pvHandler) >> 16) & 0xFFFF;
	poEntry->dwUppBaseAddr = ((QWORD)pvHandler) >> 32;
	poEntry->dwReserved = 0;
}


extern void kPrintString(int x, int y, const char* str);
void kDummyHandler(void)
{
	kPrintString(0, 15, "============= Dummy Handler Execute =============");
	while(1);
}
