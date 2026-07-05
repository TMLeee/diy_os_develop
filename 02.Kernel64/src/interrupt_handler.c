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


void kCommonExceptionHandler(int iVectorNum, QWORD qwErrCode)
{
	char vcBuffer[3] = {0,};

	// 인터럽트 백터 번호 출력
	vcBuffer[0] = '0' + (iVectorNum / 10);
	vcBuffer[1] = '0' + (iVectorNum % 10);
	vcBuffer[2] = '\0';

	kPrintStringXY(0, 0, "===============================================");
	kPrintStringXY(0, 1, "Exception Occurred! Vector: ");
	kPrintStringXY(27, 1, vcBuffer);

	while(1);
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
