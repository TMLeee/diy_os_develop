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
#include "mm.h"
#include "slab.h"
#include "vmalloc.h"
#include "task.h"


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
		{"crash", "Raise an exception, ex)crash div0|pf|gp|ud|wtext|xdata", kCrash},
		{"memmap", "Show E820 Physical Memory Map", kShowMemoryMap},
		{"pmemstat", "Show Physical Frame Allocator Stat", kShowPhysMemStat},
		{"alloctest", "Alloc/Free Frames, ex)alloctest 100 0(order)", kAllocTest},
		{"pgwalk", "Walk Page Tables, ex)pgwalk 202000", kPageWalkTest},
		{"pgtest", "Test Direct Map And Page Protection", kPageProtTest},
		{"slabinfo", "Show Slab Cache Stat", kShowSlabInfo},
		{"kmalloctest", "kmalloc/kfree Stress, ex)kmalloctest 200", kKmallocTest},
		{"vmalloctest", "vmalloc + Guard Page Test, ex)vmalloctest 4", kVmallocTest},
		{"ticks", "Show Timer Tick Count", kShowTickCount},
		{"stackoverflow", "Deliberate Kernel Stack Overflow", kStackOverflowTest},
		{"frameinfo", "Show One Frame's State, ex)frameinfo 100000", kShowFrameInfo},
		{"maptest", "Try kMapPage At A VA, ex)maptest 100000000", kMapTest}
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
		// PAGE_OFFSET(PML4[256])은 direct map이라 이제 유효하다.
		// 어느 매핑에도 속하지 않는 PML4[257] 대역을 쓴다
		kPrintf("Raising #PF...\n");
		*(volatile QWORD*)0xFFFF880000000000 = 0x1234;
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
	// W^X 확인용. .text 쓰기와 .data 실행은 각각 #PF여야 한다
	else if(0 == kMemCmp(vcType, "wtext", 5)) {
		kPrintf("Writing to .text (RO)...\n");
		*(volatile BYTE*)(KERNEL_VMA + KERNEL_PHYS_BASE) = 0x90;
	}
	else if(0 == kMemCmp(vcType, "xdata", 5)) {
		kPrintf("Executing in .data (NX)...\n");
		((void (*)(void))(QWORD)__rodata_end)();   // 고주소 별칭. NX가 걸린 쪽이다
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


// direct map이 워크만 되는 게 아니라 실제로 접근 가능한지 확인한다
void kPageProtTest(const char* poParamBuff)
{
	QWORD qwFrame;
	volatile QWORD* pqwIdent;
	volatile QWORD* pqwDirect;
	char vcHex[17];

	static const char* vpcSect[3] = {"text", "rodata", "data"};
	QWORD vqProbe[3];
	pte_t vqEntry[4];
	int i;

	kToHexString(kReadCR0(), vcHex, 16);
	kPrintf("CR0=%s WP=%s NX=%s\n", vcHex,
			(kReadCR0() & CR0_WP) ? "on" : "off",
			(TRUE == kIsNXSupported()) ? "supported" : "no");

	// 섹션 경계는 커널이 커지면 움직이므로 주소를 링커 심볼에서 가져온다
	// 보호는 실행 별칭(고주소)에 걸려 있다. identity 쪽은 여전히 RW+NX다
	vqProbe[0] = KERNEL_VMA + KERNEL_PHYS_BASE;
	vqProbe[1] = (QWORD)__text_end;
	vqProbe[2] = (QWORD)__rodata_end;

	for(i=0; i<3; ++i) {
		if(PG_LEVEL_4K != kWalkPageTable(kReadCR3(), vqProbe[i], vqEntry)) {
			kPrintf("%s: NOT 4KB\n", vpcSect[i]);
			continue;
		}
		kToHexString(vqProbe[i], vcHex, 12);
		kPrintf("%s at %s: %s%s\n", vpcSect[i], vcHex,
				(vqEntry[3] & PTE_RW) ? "RW" : "RO",
				(vqEntry[3] & PTE_NX) ? "+NX" : "+X");
	}

	qwFrame = kAllocPage();
	if(0 == qwFrame) {
		kPrintf("alloc failed\n");
		return;
	}

	pqwIdent = (volatile QWORD*)qwFrame;
	pqwDirect = (volatile QWORD*)__va(qwFrame);

	// identity로 쓰고 direct map으로 읽는다. 같은 프레임이어야 한다
	*pqwIdent = 0xFEEDFACECAFEBEEF;
	kToHexString(qwFrame, vcHex, 12);
	kPrintf("frame %s: ", vcHex);

	if(0xFEEDFACECAFEBEEF == *pqwDirect) {
		kPrintf("ident->direct OK  ");
	}
	else {
		kPrintf("ident->direct MISMATCH  ");
	}

	// 반대 방향도 확인
	*pqwDirect = 0x0123456789ABCDEF;
	if(0x0123456789ABCDEF == *pqwIdent) {
		kPrintf("direct->ident OK\n");
	}
	else {
		kPrintf("direct->ident MISMATCH\n");
	}

	kToHexString((QWORD)pqwDirect, vcHex, 16);
	kPrintf("direct map VA = %s\n", vcHex);

	kFreePage(qwFrame);
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


void kShowSlabInfo(const char* poParamBuff)
{
	kPrintSlabInfo();
}


// 여러 크기를 섞어 할당/검증/해제하고 프레임 수가 복귀하는지 본다
void kKmallocTest(const char* poParamBuff)
{
	static const QWORD vqSize[8] = {8, 24, 64, 200, 500, 1000, 3000, 20000};
	ParamList_t stList;
	char vcParam[30], vcNum[24], vcNum2[24];
	void* vpvPtr[64];
	QWORD vqUsed[64];
	int iCount, i, j, iGot;
	QWORD qwBefore, qwAfter;
	BOOL bOk = TRUE;

	kInitializeParam(&stList, poParamBuff);
	iCount = (0 == kGetNextParam(&stList, vcParam)) ? 32 : kAToI(vcParam, 10);
	if((iCount <= 0) || (64 < iCount)) {
		iCount = 32;
	}

	qwBefore = kGetFreePageCount();

	for(iGot=0; iGot<iCount; ++iGot) {
		vqUsed[iGot] = vqSize[iGot % 8];
		vpvPtr[iGot] = kmalloc(vqUsed[iGot]);
		if(NULL == vpvPtr[iGot]) {
			break;
		}
		// 요청한 크기 전체가 쓰기 가능한지 확인하며 패턴을 채운다
		kMemSet(vpvPtr[iGot], (BYTE)(0x30 + (iGot & 0x0F)), (int)vqUsed[iGot]);
	}

	// 겹쳐 쓰지 않았는지 확인
	for(i=0; i<iGot; ++i) {
		for(j=0; j<(int)vqUsed[i]; ++j) {
			if(((BYTE*)vpvPtr[i])[j] != (BYTE)(0x30 + (i & 0x0F))) {
				kPrintf("OVERLAP at block %d offset %d\n", i, j);
				bOk = FALSE;
				break;
			}
		}
		if(FALSE == bOk) {
			break;
		}
	}

	for(i=0; i<iGot; ++i) {
		kfree(vpvPtr[i]);
	}

	qwAfter = kGetFreePageCount();
	kUIToDecString(qwBefore, vcNum);
	kUIToDecString(qwAfter, vcNum2);
	kPrintf("kmalloc %d blocks, pattern %s, frames %s -> %s\n",
			iGot, (TRUE == bOk) ? "OK" : "BAD", vcNum, vcNum2);
}


// vmalloc 영역이 실제로 쓰기 가능하고, guard page가 매핑되지 않았는지 확인
void kVmallocTest(const char* poParamBuff)
{
	ParamList_t stList;
	char vcParam[30], vcHex[17];
	int iPages, i;
	QWORD qwBefore, qwAfter;
	volatile BYTE* pucBuf;
	pte_t vqEntry[4];
	QWORD qwGuard;

	kInitializeParam(&stList, poParamBuff);
	iPages = (0 == kGetNextParam(&stList, vcParam)) ? 4 : kAToI(vcParam, 10);
	if((iPages <= 0) || (64 < iPages)) {
		iPages = 4;
	}

	// 첫 vmap은 이 영역의 중간 페이지 테이블과 vmalloc_area 슬랩까지 만든다.
	// 그건 일회성 설비 비용이므로 워밍업으로 걷어내고 두 번째부터 잰다
	pucBuf = (volatile BYTE*)kVmapPages(1, 1, 1);
	if(NULL != pucBuf) {
		kVfree((void*)pucBuf);
	}

	qwBefore = kGetFreePageCount();

	// 앞뒤로 guard 한 장씩
	pucBuf = (volatile BYTE*)kVmapPages(iPages, 1, 1);
	if(NULL == pucBuf) {
		kPrintf("kVmapPages failed\n");
		return;
	}
	kToHexString((QWORD)pucBuf, vcHex, 16);
	kPrintf("vmap %d pages at %s\n", iPages, vcHex);

	// 전 범위가 쓰기 가능한지
	for(i=0; i<(iPages * PAGE_SIZE); ++i) {
		pucBuf[i] = (BYTE)(i & 0xFF);
	}
	for(i=0; i<(iPages * PAGE_SIZE); ++i) {
		if(pucBuf[i] != (BYTE)(i & 0xFF)) {
			kPrintf("DATA MISMATCH at %d\n", i);
			break;
		}
	}
	kPrintf("write/read over %d pages OK\n", iPages);

	// 앞쪽 guard가 정말 비어 있는지 (페이지 테이블로 확인, 접근하면 죽는다)
	qwGuard = (QWORD)pucBuf - PAGE_SIZE;
	kToHexString(qwGuard, vcHex, 16);
	kPrintf("guard below %s: %s\n", vcHex,
			(PG_LEVEL_NONE == kWalkPageTable(kReadCR3(), qwGuard, vqEntry))
			? "unmapped (good)" : "MAPPED (bad)");

	qwGuard = (QWORD)pucBuf + ((QWORD)iPages * PAGE_SIZE);
	kToHexString(qwGuard, vcHex, 16);
	kPrintf("guard above %s: %s\n", vcHex,
			(PG_LEVEL_NONE == kWalkPageTable(kReadCR3(), qwGuard, vqEntry))
			? "unmapped (good)" : "MAPPED (bad)");

	kVfree((void*)pucBuf);
	qwAfter = kGetFreePageCount();
	kPrintf("frames %d -> %d %s\n", (int)qwBefore, (int)qwAfter,
			(qwBefore == qwAfter) ? "OK" : "LEAK");
}


// 타이머 틱 수. 벽시계 시간과 대조하면 실제 인터럽트 주기를 잴 수 있다
void kShowTickCount(const char* poParamBuff)
{
	char vcNum[24];

	kUIToDecString(kGetTickCnt(), vcNum);
	kPrintf("ticks=%s\n", vcNum);
}


// 무한 재귀로 스택을 넘긴다. guard page가 있으면 #PF로 잡혀야 한다
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winfinite-recursion"
static QWORD kRecurse(QWORD qwDepth)
{
	volatile BYTE vcPad[256];

	vcPad[0] = (BYTE)qwDepth;
	vcPad[255] = (BYTE)qwDepth;
	return vcPad[0] + kRecurse(qwDepth + 1);
}
#pragma GCC diagnostic pop


static void kStackOverflowTask(void)
{
	kRecurse(0);
	while(1) {
		kSchedule();
	}
}


void kStackOverflowTest(const char* poParamBuff)
{
	// 셸은 EntryPoint.s의 부트 스택 위에서 돈다. 거기에는 guard page가 없어서
	// 넘쳐도 그냥 아래 메모리를 덮을 뿐이다. vmalloc 스택을 쓰는 태스크를
	// 따로 만들어 그 위에서 넘겨야 guard page를 실제로 시험할 수 있다
	if(NULL == kCreateTask(0, (QWORD)kStackOverflowTask)) {
		kPrintf("task creation failed\n");
		return;
	}
	kPrintf("overflow task created; expect a guard-page #PF\n");
}


// 프레임 하나의 상태를 그대로 보여 준다. 할당 순서에 의존하지 않고
// "이 물리주소가 할당 가능한가"를 직접 확인할 수 있다
void kShowFrameInfo(const char* poParamBuff)
{
	ParamList_t stList;
	char vcParam[30], vcHex[17];
	QWORD qwPhysAddr;
	page_t* poPage;

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) frameinfo 100000\n");
		return;
	}

	qwPhysAddr = (QWORD)kAToI(vcParam, 16);
	poPage = kPhysToPage(qwPhysAddr);
	if(NULL == poPage) {
		kPrintf("no page_t for that address\n");
		return;
	}

	kToHexString(PAGE_ALIGN_DOWN(qwPhysAddr), vcHex, 12);
	kPrintf("frame %s: %s %s%s%s order=%d ref=%d\n", vcHex,
			(poPage->qwFlags & PG_RESERVED) ? "RESERVED" : "allocatable",
			(poPage->qwFlags & PG_BUDDY) ? "buddy-free " : "",
			(poPage->qwFlags & PG_SLAB) ? "slab " : "",
			(0 == poPage->qwFlags) ? "in-use " : "",
			poPage->iOrder, poPage->iRefCount);
}


// 임의의 가상주소에 프레임을 매핑해 본다. 유저 공간을 어디에 둘 수 있는지
// 판단하려면 "여기에 kMapPage가 되는가"를 실측해야 한다
void kMapTest(const char* poParamBuff)
{
	ParamList_t stList;
	char vcParam[30], vcHex[17];
	QWORD qwVirtAddr, qwPhys;
	volatile QWORD* pqw;

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) maptest 100000000\n");
		return;
	}
	qwVirtAddr = PAGE_ALIGN_DOWN((QWORD)kAToI(vcParam, 16));

	qwPhys = kAllocPage();
	if(0 == qwPhys) {
		kPrintf("alloc failed\n");
		return;
	}

	kToHexString(qwVirtAddr, vcHex, 16);
	if(FALSE == kMapPage(kReadCR3(), qwVirtAddr, qwPhys,
				PTE_RW | (kIsNXSupported() ? PTE_NX : 0))) {
		kPrintf("VA %s: kMapPage FAILED (blocked by an existing 2MB page)\n", vcHex);
		kFreePage(qwPhys);
		return;
	}

	pqw = (volatile QWORD*)qwVirtAddr;
	*pqw = 0xC0FFEE0000BEEF;
	kPrintf("VA %s: mapped, readback %s\n", vcHex,
			(0xC0FFEE0000BEEF == *pqw) ? "OK" : "MISMATCH");

	kUnmapPage(kReadCR3(), qwVirtAddr);
	kFreePage(qwPhys);
}
