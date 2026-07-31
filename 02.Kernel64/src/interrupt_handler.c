/*
 * interrupt_handler.c
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */



#include "interrupt_handler.h"
#include "pic.h"
#include "keyboard.h"
#include "console.h"
#include "utility.h"
#include "task.h"
#include "descriptor.h"
#include "panic.h"
#include "vmalloc.h"
#include "assembly_utils.h"


void kTimerHandler(int iVectorNum)
{
	char vcBuf[] = "[INT:  , ]";
    static int g_iTimerIntCnt = 0;

    vcBuf[ 5 ] = '0' + iVectorNum / 10;
    vcBuf[ 6 ] = '0' + iVectorNum % 10;
    vcBuf[ 8 ] = '0' + g_iTimerIntCnt;
    g_iTimerIntCnt = ( g_iTimerIntCnt + 1 ) % 10;
    kPrintStringXY( 70, 0, vcBuf );

	kSendEOIToPIC(iVectorNum - PIC_IRQ_START_VECTOR);

	++g_qwTickCount;

	kDecreaseProcessorTime();
	if(TRUE == kIsProcessorTimeExpired()) {
		kScheduleInInterrunt();
	}
}


// isr.asm이 넘기는 인자: RDI=벡터, RSI=에러코드(없으면 0), RDX=레지스터 프레임
void kCommonExceptionHandler(int iVectorNum, QWORD qwErrCode, QWORD* pqwFrame)
{
	BOOL bHasErrCode;

	kDisableInterrupt();

	bHasErrCode = kIsExceptionHasErrCode(iVectorNum);
	if(FALSE == bHasErrCode) {
		qwErrCode = 0;
	}

	kDumpRegisters(pqwFrame, iVectorNum, qwErrCode, bHasErrCode);

	// #PF가 vmalloc guard page를 짚었다면 십중팔구 커널 스택 오버플로다
	if((14 == iVectorNum) && (TRUE == kIsVmallocGuardPage(kReadCR2()))) {
		kPanic("KERNEL STACK OVERFLOW - CR2 is in a vmalloc guard page");
	}
	kPanic("Unhandled exception %d (%s)", iVectorNum, kGetExceptionName(iVectorNum));
}


void kCommonInterruptHandler(int iVectorNum)
{
	char vcBuffer[11] = "[INT:  , ]";
	static int g_iCommonIntCnt = 0;

	vcBuffer[5] = '0' + (iVectorNum / 10);
	vcBuffer[6] = '0' + (iVectorNum % 10);

	vcBuffer[8] = '0' + g_iCommonIntCnt;
	g_iCommonIntCnt = (g_iCommonIntCnt + 1) % 10;
	kPrintStringXY(70, 0, vcBuffer);

	kSendEOIToPIC(iVectorNum - PIC_IRQ_START_VECTOR);
}


void kKeyboardHandler(int iVectorNum)
{
	char vcBuffer[11] = "[INT:  , ]";
	static int g_iKeyIntCnt = 0;
	BYTE ucTemp;

	// 핸들러 로그 출력
	vcBuffer[5] = '0' + (iVectorNum / 10);
	vcBuffer[6] = '0' + (iVectorNum % 10);

	vcBuffer[8] = '0' + g_iKeyIntCnt;
	g_iKeyIntCnt = (g_iKeyIntCnt + 1) % 10;
	kPrintStringXY(0, 0, vcBuffer);


	// 키보드 값을 읽어서 큐에 삽입
	if(TRUE == kIsOutputBufferFull()) {
		ucTemp = kGetKeyboardScanCode();
		kConvertScanCodeAndPutQueue(ucTemp);
	}

	kSendEOIToPIC(iVectorNum - PIC_IRQ_START_VECTOR);
}
