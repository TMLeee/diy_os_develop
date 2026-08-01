/*
 * descriptor.c
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */


#include "descriptor.h"
#include "utility.h"
#include "isr.h"


// ring3 -> ring0 전이 때 CPU가 여기서 RSP를 가져온다. 태스크 전환마다 갱신해야 한다
static TSSSegment_t* g_poTSS = NULL;


void kSetTSSRsp0(QWORD qwRsp0)
{
	if(NULL != g_poTSS) {
		g_poTSS->qwRsp[0] = qwRsp0;
	}
}


void kInitGDTTableAndTSS(void)
{
	GDTR* poGDTR;
	GDTEntry8_t* poEntry;
	TSSSegment_t* poTSS;

	// GDTR 설정
	poGDTR = (GDTR*) GDTR_START_ADDR;
	poEntry = (GDTEntry8_t*) (GDTR_START_ADDR + sizeof(GDTR));
	poGDTR->wLimit = GDT_TBL_SIZE - 1;
	poGDTR->qwBaseAddr = (QWORD)poEntry;

	// TSS 영역 설정
	poTSS = (TSSSegment_t*) ((QWORD)poEntry + GDT_TBL_SIZE);

	// NULL, KCode, KData, UData, UCode, TSS
	// poTSS는 물리주소(GDTR_START_ADDR=0x142000)이고 identity 매핑이므로
	// 디스크립터 베이스에 그대로 넣는다. __va()/__pa()를 끼우면 안 된다
	kSetGDTEntry8(&(poEntry[0]), 0, 0, 0, 0, 0);
	kSetGDTEntry8(&(poEntry[1]), 0, 0xFFFFF, GDT_FLAG_UPPER_CODE, GDT_FLAG_LOWER_KERNELCODE, GDT_TYPE_CODE);
	kSetGDTEntry8(&(poEntry[2]), 0, 0xFFFFF, GDT_FLAG_UPPER_DATA, GDT_FLAG_LOWER_KERNELDATA, GDT_TYPE_DATA);
	kSetGDTEntry8(&(poEntry[3]), 0, 0xFFFFF, GDT_FLAG_UPPER_DATA, GDT_FLAG_LOWER_USERDATA, GDT_TYPE_DATA);
	kSetGDTEntry8(&(poEntry[4]), 0, 0xFFFFF, GDT_FLAG_UPPER_CODE, GDT_FLAG_LOWER_USERCODE, GDT_TYPE_CODE);

	// 16바이트 TSS 디스크립터는 8바이트 슬롯 두 개를 먹는다.
	// 인덱스를 상수로 박지 말고 GDTR_8BYTE_ENT_SIZE에서 유도한다
	kSetGDTEntry16((GDTEntry16_t*) &(poEntry[GDTR_8BYTE_ENT_SIZE]),
					(QWORD)poTSS, sizeof(TSSSegment_t) - 1, GDT_FLAG_UPPER_TSS,
					GDT_FLAG_LOWER_TSS, GDT_TYPE_TSS);

	// TSS 초기화
	g_poTSS = poTSS;
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

	// Exception ISR 등록
	kSetIDTEntry(&(poEntry[0]), kISRDivideError, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[1]), kISRDebug, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[2]), kISRNMI, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[3]), kISRBreakPoint, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[4]), kISROverFlow, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[5]), kISRBoundRangeExceeded, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[6]), kISRInvalidOpcode, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[7]), kISRDeviceNotAvailable, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[8]), kISRDoubleFault, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[9]), kISRCoprocessorSegmentOverrun, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[10]), kISRInvalidTSS, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[11]), kISRSegmentNotPresent, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[12]), kISRStackSegmentFault, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[13]), kISRGeneralProtection, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[14]), kISRPageFault, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[15]), kISR15, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[16]), kISRFPUError, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[17]), kISRAlignmentCheck, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[18]), kISRMachineCheck, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[19]), kISRSIMDError, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[20]), kISRETCException, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	for(i=21; i<32; ++i) {
		kSetIDTEntry(&(poEntry[i]), kISRETCException, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	}

	// Interrupt ISR 등록
	kSetIDTEntry(&(poEntry[32]), kISRTimer, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[33]), kISRKeyboard, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[34]), kISRSlavePIC, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[35]), kISRSerial2, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[36]), kISRSerial1, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[37]), kISRParallel2, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[38]), kISRFloppy, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[39]), kISRParallel1, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[40]), kISRRTC, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[41]), kISRReserved, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[42]), kISRNotUsed1, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[43]), kISRNotUsed2, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[44]), kISRMouse, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[45]), kISRCoprocessor, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[46]), kISRHDD1, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	kSetIDTEntry(&(poEntry[47]), kISRHDD2, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	// IDT_TBL_SIZE는 바이트 수(1600). 여기는 엔트리 인덱스다
	for(i=48; i<IDT_ENTRY_SIZE; ++i) {
		kSetIDTEntry(&(poEntry[i]), kISRETCInterrupt, 0x08, IDT_FLAG_IST1, IDT_FLAG_KENREL, IDT_TYPE_INTERRUPT);
	}

}


void kSetIDTEntry(IDTEntry_t *poEntry, void* pvHandler, WORD wSelector,
				BYTE ucIST, BYTE ucFlag, BYTE ucType)
{
	// IDT Gate Decriptor 설정
	poEntry->wLowBaseAddr = (QWORD)pvHandler & 0xFFFF;
	poEntry->wSegSelector = wSelector;
	poEntry->ucIST = ucIST & 0x07;		// IST 필드는 3비트
	poEntry->ucTypeAndFlag = ucType | ucFlag;
	poEntry->wMidBaseAddr = (((QWORD)pvHandler) >> 16) & 0xFFFF;
	poEntry->dwUppBaseAddr = ((QWORD)pvHandler) >> 32;
	poEntry->dwReserved = 0;
}
