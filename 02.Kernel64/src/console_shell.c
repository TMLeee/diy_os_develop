/*
 * console_shell.c
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */

#include "console_shell.h"
#include "console.h"
#include "keyboard.h"
#include "utility.h"
#include "pit.h"
#include "rtc.h"
#include "assembly_utils.h"
#include "serial.h"
#include "memmap.h"
#include "pmm.h"
#include "paging.h"


ShellCmdEntry_t gtCommandTable[] =
{
		{"help", "Show Help", kHelp},
		{"cls", "Clear Screen", kCls},
		{"totalram", "Show Total RAM Size", kShowTotalRAMSize},
		{"strtod", "String to Decimal/Hex Convert", kStringToDecimalHexTest},
		{"shutdown", "Showdown and Reboot System", kShutdown},
		{"settimer", "Set PIT Controller Counter0, ex)settimer 10[ms] 1[periodic]", kSetTimer},
		{"wait", "Wait ms Using PIT, ex)wait 100[ms]", kWaitUsingPIT},
		{"rdtsc", "Read Time Stamp Counter", kReadTimeStampCounter},
		{"cpuspeed", "Measure Processor Speed", kMeasureProcessorSpeed},
		{"date", "Show Data and Time", kShowDateAndTime},
		{"createtask", "Create Task, ex)createtask 1(type) 10(count)", kCreateTestTask},
		{"crash", "Raise an exception, ex)crash div0|pf|gp|ud", kCrash},
		{"memmap", "Show E820 Physical Memory Map", kShowMemoryMap},
		{"pmemstat", "Show Physical Frame Allocator Stat", kShowPhysMemStat},
		{"alloctest", "Alloc/Free Frames, ex)alloctest 100 0(order)", kAllocTest},
		{"pgwalk", "Walk Page Tables, ex)pgwalk 202000", kPageWalkTest}
};


void kStartConsoleShell(void)
{
	char vcCmdBuff[CONSOLESHELL_MAX_COM_BUFF_SIZE];
	int iCmdBuffIdx = 0;
	BYTE ucKey;
	int iCursorX, iCursorY;

	// 프롬포트 출력
	kPrintf(CONSOLESHELL_PROG_MSG);

	while(1) {
		ucKey = kGetch();

		// Backspace 처리
		if(KEY_BACKSPACE == ucKey) {
			if(0 < iCmdBuffIdx) {
				kGetCursor(&iCursorX, &iCursorY);
				kPrintStringXY(iCursorX-1, iCursorY, " ");
				kSetCursor(iCursorX-1, iCursorY);
				--iCmdBuffIdx;

				// 이 경로는 kConsolePrintString을 안 거치므로 시리얼은 직접 지운다
				kSerialPutString("\b \b");
			}
		}
		// Enter 처리
		else if(KEY_ENTER == ucKey) {
			kPrintf("\n");

			// 커멘드 문자가 있는 경우 실행
			if(0 < iCmdBuffIdx) {
				vcCmdBuff[iCmdBuffIdx] = '\0';
				kExecuteCmd(vcCmdBuff);
			}

			kPrintf("%s", CONSOLESHELL_PROG_MSG);
			kMemSet(vcCmdBuff, '\0', CONSOLESHELL_MAX_COM_BUFF_SIZE);
			iCmdBuffIdx = 0;
		}
		// Shift, Caps Lock, Scroll Lock은 무시
		else if((KEY_LSHIFT == ucKey) || (KEY_RSHIFT == ucKey) ||
				(KEY_CAPSLOCK == ucKey) || (KEY_NUMLOCK == ucKey) ||
				(KEY_SCROLLLOCK == ucKey)) {

		}
		else {
			// Tap 처리
			if(KEY_TAB == ucKey) {
				ucKey = ' ';
			}

			// 버퍼 공간이 있는 경우 실행
			if(iCmdBuffIdx < CONSOLESHELL_MAX_COM_BUFF_SIZE) {
				vcCmdBuff[iCmdBuffIdx++] = ucKey;
				kPrintf("%c", ucKey);
			}
		}
	}
}


void kExecuteCmd(const char* poCmdBuff)
{
	int i, iSpaceIdx;
	int iCmdBuffLength, iCmdLength;
	int iCnt;

	// 공백으로 커멘드 추출
	iCmdBuffLength = kStrLen(poCmdBuff);
	for(iSpaceIdx = 0; iSpaceIdx < iCmdBuffLength; ++iSpaceIdx) {
		if(' ' == poCmdBuff[iSpaceIdx]) {
			break;
		}
	}

	// 커멘드 테이블 검사
	iCnt = sizeof(gtCommandTable) / sizeof(ShellCmdEntry_t);
	for(i=0; i<iCnt; ++i) {
		iCmdLength = kStrLen(gtCommandTable[i].strCmd);
		if((iCmdLength == iSpaceIdx) &&
				(kMemCmp(gtCommandTable[i].strCmd, poCmdBuff, iSpaceIdx) == 0)) {
			gtCommandTable[i].pfFunc(poCmdBuff + iSpaceIdx + 1);
			break;
		}
	}

	// 리스트에 없는 경우 에러
	if(iCnt <= i) {
		kPrintf("'%s' is not command.\n", poCmdBuff);
	}
}


void kInitializeParam(ParamList_t* poList, const char* poParam)
{
	poList->poBuff 	= poParam;
	poList->iLength	= kStrLen(poParam);
	poList->iCurPos	= 0;
}


int kGetNextParam(ParamList_t* poList, char* poParam)
{
	int i=0;
	int iLength;

	// 파라미터가 없는 경우
	if(poList->iLength <= poList->iCurPos) {
		return 0;
	}

	// 공백 검색
	for(i=poList->iCurPos; i<poList->iLength; ++i) {
		if(' ' == poList->poBuff[i]) {
			break;
		}
	}

	// 파라미터 복사
	kMemCpy(poParam, poList->poBuff + poList->iCurPos, i);
	iLength = i - poList->iCurPos;
	poParam[iLength] = '\0';

	// 파라미터 위치 업데이트
	poList->iCurPos += iLength + 1;
	return iLength;
}


void kHelp(const char* poParamBuff)
{
	int i;
	int iCnt;
	int iCursorX, iCursorY;
	int iLength, iMaxCmdLength = 0;

	kPrintf("\nTM OS Help\n");
	iCnt = sizeof(gtCommandTable) / sizeof(ShellCmdEntry_t);

	for(i=0; i<iCnt; ++i) {
		iLength = kStrLen(gtCommandTable[i].strCmd);
		if(iMaxCmdLength < iLength) {
			iMaxCmdLength = iLength;
		}
	}

	// 도움말 출력
	for(i=0; i<iCnt; ++i) {
		kPrintf("%s", gtCommandTable[i].strCmd);
		kGetCursor(&iCursorX, &iCursorY);
		kSetCursor(iMaxCmdLength, iCursorY);
		kPrintf(" - %s\n", gtCommandTable[i].strHelp);
	}
}


void kCls(const char* poParamBuff)
{
	kCleanScreen();
	kSetCursor(0, 1);
}


void kShowTotalRAMSize(const char* poParamBuff)
{
	kPrintf("Total RAM Size: %d MB\n", kGetTotalRAMSize());
}


void kStringToDecimalHexTest(const char* poParamBuff)
{

}


void kShutdown(const char* poParamBuff)
{
	kPrintf("System Shutdown Start...\n");
	kPrintf("Press Any Key to Reboot System");
	kGetch();
	kReboot();
}


void kSetTimer(const char* poParamBuff)
{
	char vcParam[100];
	ParamList_t stList;
	long lValue;
	BOOL bPeriodic;

	kInitializeParam(&stList, poParamBuff);

	// milisecond (두 번째 인자는 출력 버퍼)
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) settimer 10[ms] 1[periodic]\n");
		return;
	}
	lValue = kAToI(vcParam, 10);

	// Periodic
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) settimer 10[ms] 1[periodic]\n");
		return;
	}
	bPeriodic = kAToI(vcParam, 10);

	kInitializePIT(MS_TO_COUNT(lValue), bPeriodic);
	kPrintf("Time=%d[ms] Periodic=%d Change Complete\n", lValue, bPeriodic);
}


void kWaitUsingPIT(const char* poParamBuff)
{
	char vcParam[100];
	ParamList_t stList;
	long lMilisecond;
	int i;

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcParam)){
		kPrintf("ex) wait 100[ms]\n");
		return;
	}
	lMilisecond = kAToI(vcParam, 10);
	kPrintf("%d[ms] Sleep Start...\n", lMilisecond);

	// 인터럽트 비활성화
	kDisableInterrupt();
	for(i=0; i<lMilisecond/30; ++i) {
		kWaitUsingDirectPIT(MS_TO_COUNT(30));
	}
	kWaitUsingDirectPIT(MS_TO_COUNT(lMilisecond % 30));
	kEnableInterrupt();
	kPrintf("%d[ms] Sleep Complete\n", lMilisecond);

	// 타이머 복원
	kInitializePIT(MS_TO_COUNT(1), TRUE);
}


void kReadTimeStampCounter(const char* poParamBuff)
{
	QWORD qwTSC;

	qwTSC = kReadTSC();
	kPrintf("Time Stamp Counter = %q\n", qwTSC);
}


void kMeasureProcessorSpeed(const char* poParamBuff)
{
	int i;
	QWORD qwLastTSC, qwTotalTSC = 0;

	kPrintf("Now Measuring,");

	// 10초간 카운트 값으로 프로세서 클럭 측정
	kDisableInterrupt();
	for(i=0; i<200; ++i) {
		qwLastTSC = kReadTSC();
		kWaitUsingDirectPIT(MS_TO_COUNT(50));
		qwTotalTSC += kReadTSC() - qwLastTSC;

		kPrintf(".");
	}

	// 타이머 복원
	kInitializePIT(MS_TO_COUNT(1), TRUE);
	kEnableInterrupt();

	kPrintf("\nCPU Speed=%d[MHz]\n", qwTotalTSC / 10/ 1000 / 1000);
}


void kShowDateAndTime(const char* poParamBuff)
{
	BYTE ucSecond, ucMinute, ucHour;
	BYTE ucDayOfWeek, ucDayOfMonth, ucMonth;
	WORD wYear;

	// RTC로 부터 시간, 날짜 읽기
	kReadRTCTime(&ucHour, &ucMinute, &ucSecond);
	kReadRTCData(&wYear, &ucMonth, &ucDayOfMonth, &ucDayOfWeek);

	kPrintf("Data: %d/%d/%d %s, ", wYear, ucMonth, ucDayOfMonth, kConvDayOfWeekToString(ucDayOfWeek));
	kPrintf("Time: %d:%d:%d\n", ucHour, ucMinute, ucSecond);
}


void kTestTask1( void )
{
    BYTE bData;
    int i = 0, iX = 0, iY = 0, iMargin;
    CharStruct* pstScreen = ( CharStruct* ) CONSOLE_VIDEO_MEM_ADDR;
    TCB_t* pstRunningTask;
    
    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    pstRunningTask = kGetRunningTask();
    iMargin = ( pstRunningTask->stLink.qwID & 0xFFFFFFFF ) % 10;
    
    // 화면 네 귀퉁이를 돌면서 문자 출력
    while( 1 )
    {
        switch( i )
        {
        case 0:
            iX++;
            if( iX >= ( CONSOLE_WIDTH - iMargin ) )
            {
                i = 1;
            }
            break;
            
        case 1:
            iY++;
            if( iY >= ( CONSOLE_HEIGHT - iMargin ) )
            {
                i = 2;
            }
            break;
            
        case 2:
            iX--;
            if( iX < iMargin )
            {
                i = 3;
            }
            break;
            
        case 3:
            iY--;
            if( iY < iMargin )
            {
                i = 0;
            }
            break;
        }
        
        // 문자 및 색깔 지정
        pstScreen[ iY * CONSOLE_WIDTH + iX ].ucChar = bData;
        pstScreen[ iY * CONSOLE_WIDTH + iX ].ucAttr = bData & 0x0F;
        bData++;
        
        // 다른 태스크로 전환
        kSchedule();
    }
}


void kTestTask2( void )
{
    int i = 0, iOffset;
    CharStruct* pstScreen = ( CharStruct* ) CONSOLE_VIDEO_MEM_ADDR;
    TCB_t* pstRunningTask;
    char vcData[ 4 ] = { '-', '\\', '|', '/' };
    
    // 자신의 ID를 얻어서 화면 오프셋으로 사용
    pstRunningTask = kGetRunningTask();
    iOffset = ( pstRunningTask->stLink.qwID & 0xFFFFFFFF ) * 2;
    iOffset = CONSOLE_WIDTH * CONSOLE_HEIGHT - 
        ( iOffset % ( CONSOLE_WIDTH * CONSOLE_HEIGHT ) );

    while( 1 )
    {
        // 회전하는 바람개비를 표시
        pstScreen[ iOffset ].ucChar = vcData[ i % 4 ];
        // 색깔 지정
        pstScreen[ iOffset ].ucAttr = ( iOffset % 15 ) + 1;
        i++;
        
        // 다른 태스크로 전환
        kSchedule();
    }
}


void kCreateTestTask(const char* poParamBuff)
{
	ParamList_t stList;
    char vcType[ 30 ];
    char vcCount[ 30 ];
    int i;
    
    // 파라미터를 추출
    kInitializeParam( &stList, poParamBuff );
    kGetNextParam( &stList, vcType );
    kGetNextParam( &stList, vcCount );

    switch( kAToI( vcType, 10 ) )
    {
    // 타입 1 태스크 생성
    case 1:
        for( i = 0 ; i < kAToI( vcCount, 10 ) ; i++ )
        {    
            if( kCreateTask( 0, ( QWORD ) kTestTask1 ) == NULL )
            {
                break;
            }
        }
        
        kPrintf( "Task1 %d Created\n", i );
        break;
        
    // 타입 2 태스크 생성
    case 2:
    default:
        for( i = 0 ; i < kAToI( vcCount, 10 ) ; i++ )
        {    
            if( kCreateTask( 0, ( QWORD ) kTestTask2 ) == NULL )
            {
                break;
            }
        }
        
        kPrintf( "Task2 %d Created\n", i );
        break;
    }
}


// 예외 덤프 경로를 직접 확인하기 위해 일부러 예외를 일으킨다
void kCrash(const char* poParamBuff)
{
	ParamList_t stList;
	char vcType[30] = {0,};
	// 양쪽 모두 volatile이어야 한다. 분자가 상수면 GCC가 나눗셈을 접어버린다
	volatile int iNum = 1;
	volatile int iZero = 0;
	volatile int iResult;

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcType)) {
		kPrintf("ex) crash div0|pf|gp|ud\n");
		return;
	}

	if(0 == kMemCmp(vcType, "div0", 4)) {
		kPrintf("Raising #DE...\n");
		iResult = iNum / iZero;
		kPrintf("no fault: %d\n", iResult);
	}
	else if(0 == kMemCmp(vcType, "pf", 2)) {
		kPrintf("Raising #PF...\n");
		*(volatile QWORD*)0xFFFF800000000000 = 0x1234;
	}
	else if(0 == kMemCmp(vcType, "gp", 2)) {
		kPrintf("Raising #GP...\n");
		// GDT 한계(0x27)를 넘는 셀렉터를 적재
		__asm__ __volatile__ ("mov $0x50, %%ax; mov %%ax, %%ds" ::: "rax");
	}
	else if(0 == kMemCmp(vcType, "ud", 2)) {
		kPrintf("Raising #UD...\n");
		__asm__ __volatile__ ("ud2");
	}
	else {
		kPrintf("ex) crash div0|pf|gp|ud\n");
	}
}


void kShowMemoryMap(const char* poParamBuff)
{
	kPrintMemoryMap();
}


void kShowPhysMemStat(const char* poParamBuff)
{
	kPrintPhysicalMemoryStat();
}


void kPageWalkTest(const char* poParamBuff)
{
	ParamList_t stList;
	char vcParam[30];
	QWORD qwVirtAddr;

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) pgwalk 202000\n");
		return;
	}

	qwVirtAddr = (QWORD)kAToI(vcParam, 16);
	kDumpPageWalk(kReadCR3(), qwVirtAddr);
}


// n개를 할당했다가 전부 해제하고, free 카운트가 정확히 복귀하는지 본다
void kAllocTest(const char* poParamBuff)
{
	ParamList_t stList;
	char vcParam[30];
	int iCount, iOrder, i, iGot;
	QWORD qwBefore, qwAfter;
	QWORD vqAddr[64];
	char vcNum[24], vcHex[17];

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) alloctest 100 0\n");
		return;
	}
	iCount = kAToI(vcParam, 10);
	iOrder = (0 == kGetNextParam(&stList, vcParam)) ? 0 : kAToI(vcParam, 10);

	if((iCount <= 0) || (64 < iCount) || (iOrder < 0) || (10 < iOrder)) {
		kPrintf("count 1..64, order 0..10\n");
		return;
	}

	qwBefore = kGetFreePageCount();

	for(iGot=0; iGot<iCount; ++iGot) {
		vqAddr[iGot] = kAllocPages(iOrder);
		if(0 == vqAddr[iGot]) {
			break;
		}
		// 정렬 확인 후 패턴을 써 본다
		if(0 != (vqAddr[iGot] & ((1UL << iOrder) * PAGE_SIZE - 1))) {
			kPrintf("MISALIGNED at %d\n", iGot);
			break;
		}
		*(volatile QWORD*)vqAddr[iGot] = 0xA5A5A5A5A5A5A5A5;
	}

	kUIToDecString((QWORD)iGot, vcNum);
	kToHexString(vqAddr[0], vcHex, 12);
	kPrintf("allocated %s (order %d) first=%s\n", vcNum, iOrder, vcHex);

	// 패턴 검증 후 해제
	for(i=0; i<iGot; ++i) {
		if(0xA5A5A5A5A5A5A5A5 != *(volatile QWORD*)vqAddr[i]) {
			kPrintf("PATTERN CORRUPT at %d\n", i);
		}
		kFreePages(vqAddr[i], iOrder);
	}

	qwAfter = kGetFreePageCount();
	kUIToDecString(qwBefore, vcNum);
	kUIToDecString(qwAfter, vcHex);
	kPrintf("free before=%s after=%s %s\n", vcNum, vcHex,
			(qwBefore == qwAfter) ? "OK" : "LEAK");
}
