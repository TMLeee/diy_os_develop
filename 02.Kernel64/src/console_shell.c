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
#include "syscall.h"
#include "mm_struct.h"


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
		{"maptest", "Try kMapPage At A VA, ex)maptest 100000000", kMapTest},
		{"syscalltest", "Exercise The int 0x80 Path", kSyscallTest},
		{"mmtest", "Build A User Address Space And Switch To It", kMmTest},
		{"cr3test", "Run A Task Bound To Its Own Address Space", kCR3Test}
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
	volatile QWORD* pqwAlias;
	volatile QWORD* pqwDirect;
	// 커널이 쓰지 않는 저주소 한 장. 유저 텍스트가 앉을 자리이기도 하다
	#define PGTEST_ALIAS_VA	0x400000UL
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
	// 커널이 실제로 실행되는 고주소 별칭을 본다. 보호는 거기에 걸려 있다
	vqProbe[0] = KERNEL_VMA + KERNEL_PHYS_BASE;
	vqProbe[1] = (QWORD)__text_end;
	vqProbe[2] = (QWORD)__rodata_end;

	for(i=0; i<3; ++i) {
		if(PG_LEVEL_4K != kWalkPageTable(kReadCR3(), vqProbe[i], vqEntry)) {
			kPrintf("%s: NOT 4KB\n", vpcSect[i]);
			continue;
		}
		kToHexString(vqProbe[i], vcHex, 16);   // higher-half 주소라 12자리로는 잘린다
		kPrintf("%s at %s: %s%s\n", vpcSect[i], vcHex,
				(vqEntry[3] & PTE_RW) ? "RW" : "RO",
				(vqEntry[3] & PTE_NX) ? "+NX" : "+X");
	}

	qwFrame = kAllocPage();
	if(0 == qwFrame) {
		kPrintf("alloc failed\n");
		return;
	}

	// identity 별칭은 이제 없다. 대신 저주소에 4KB 별칭을 직접 만들어
	// direct map과 같은 프레임을 보는지 확인한다. identity를 걷어낸 덕분에
	// 이 매핑이 가능해졌다는 것까지 한 번에 검사된다
	pqwDirect = (volatile QWORD*)__va(qwFrame);
	kToHexString(qwFrame, vcHex, 12);
	kPrintf("frame %s: ", vcHex);

	if(FALSE == kMapPage(kReadCR3(), PGTEST_ALIAS_VA, qwFrame, PTE_RW | PTE_NX)) {
		kPrintf("low alias map FAILED\n");
		kFreePage(qwFrame);
		return;
	}
	pqwAlias = (volatile QWORD*)PGTEST_ALIAS_VA;

	*pqwAlias = 0xFEEDFACECAFEBEEF;
	kPrintf("%s  ", (0xFEEDFACECAFEBEEF == *pqwDirect) ? "alias->direct OK" : "alias->direct MISMATCH");

	// 반대 방향도 확인
	*pqwDirect = 0x0123456789ABCDEF;
	kPrintf("%s\n", (0x0123456789ABCDEF == *pqwAlias) ? "direct->alias OK" : "direct->alias MISMATCH");

	kToHexString((QWORD)pqwDirect, vcHex, 16);
	kPrintf("direct map VA = %s\n", vcHex);

	// 저주소가 정말 비어 있어야 유저 주소공간이 들어간다
	kUnmapPage(kReadCR3(), PGTEST_ALIAS_VA);
	kPrintf("low half %s\n",
			(0 == kVirtToPhys(kReadCR3(), PGTEST_ALIAS_VA)) ? "clear (no identity map)" : "STILL MAPPED");

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
		// kAllocPages는 물리주소를 준다. 만지려면 direct map을 거쳐야 한다
		*(volatile QWORD*)__va(vqAddr[iGot]) = 0xA5A5A5A5A5A5A5A5;
	}

	kUIToDecString((QWORD)iGot, vcNum);
	kToHexString(vqAddr[0], vcHex, 12);
	kPrintf("allocated %s (order %d) first=%s\n", vcNum, iOrder, vcHex);

	// 패턴 검증 후 해제
	for(i=0; i<iGot; ++i) {
		if(0xA5A5A5A5A5A5A5A5 != *(volatile QWORD*)__va(vqAddr[i])) {
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
	QWORD qwVirtAddr, qwPhys, qwUS = 0;
	volatile QWORD* pqw;
	pte_t vqEntry[4];
	int i, iLevel, iUserLevels = 0;

	kInitializeParam(&stList, poParamBuff);
	if(0 == kGetNextParam(&stList, vcParam)) {
		kPrintf("ex) maptest 100000000 [user]\n");
		return;
	}
	qwVirtAddr = PAGE_ALIGN_DOWN((QWORD)kAToI(vcParam, 16));

	// 두 번째 인자가 있으면 유저 페이지로 매핑한다. x86-64는 네 레벨의 U/S를
	// AND하므로 중간 레벨까지 US가 서 있어야 ring3에서 닿는다
	if(0 != kGetNextParam(&stList, vcParam)) {
		qwUS = PTE_US;
	}

	qwPhys = kAllocPage();
	if(0 == qwPhys) {
		kPrintf("alloc failed\n");
		return;
	}

	kToHexString(qwVirtAddr, vcHex, 16);
	if(FALSE == kMapPage(kReadCR3(), qwVirtAddr, qwPhys,
				PTE_RW | qwUS | (kIsNXSupported() ? PTE_NX : 0))) {
		kPrintf("VA %s: kMapPage FAILED (blocked by an existing 2MB page)\n", vcHex);
		kFreePage(qwPhys);
		return;
	}

	pqw = (volatile QWORD*)qwVirtAddr;
	*pqw = 0xC0FFEE0000BEEF;
	kPrintf("VA %s: mapped, readback %s\n", vcHex,
			(0xC0FFEE0000BEEF == *pqw) ? "OK" : "MISMATCH");

	// 중간 레벨까지 US가 전파됐는지 센다. 리프만 US면 ring3에서 못 닿는다
	iLevel = kWalkPageTable(kReadCR3(), qwVirtAddr, vqEntry);
	if(PG_LEVEL_4K == iLevel) {
		for(i=0; i<4; ++i) {
			if(vqEntry[i] & PTE_US) {
				++iUserLevels;
			}
		}
		kPrintf("US levels %d/4 (want %d)\n", iUserLevels, qwUS ? 4 : 0);
	}

	kUnmapPage(kReadCR3(), qwVirtAddr);
	kFreePage(qwPhys);
}

// ring0에서 int 0x80을 직접 쳐서 게이트/디스패처/반환값 경로를 확인한다.
// ring3 진입은 스텝 25d에서 붙는다
void kSyscallTest(const char* poParamBuff)
{
	const char* pcMsg = "hello from int 0x80\n";
	QWORD qwBefore, qwRet;

	qwBefore = kGetSyscallCount();

	qwRet = kDoSyscall(SYS_WRITE, 1, (QWORD)pcMsg, kStrLen(pcMsg));
	kPrintf("sys_write  -> %d (len %d)\n", (int)qwRet, kStrLen(pcMsg));

	qwRet = kDoSyscall(SYS_GETPID, 0, 0, 0);
	kPrintf("sys_getpid -> %q  running=%q  %s\n", qwRet,
			kGetRunningTask()->stLink.qwID,
			(qwRet == kGetRunningTask()->stLink.qwID) ? "MATCH" : "MISMATCH");

	qwRet = kDoSyscall(SYS_UPTIME, 0, 0, 0);
	kPrintf("sys_uptime -> %q ticks\n", qwRet);

	// 없는 번호는 -ENOSYS. 부호 확장이 살아 있는지도 같이 본다
	qwRet = kDoSyscall(4242, 0, 0, 0);
	kPrintf("bad call   -> %d (want -38)\n", (int)qwRet);

	qwRet = kDoSyscall(SYS_WRITE, 99, (QWORD)pcMsg, 4);
	kPrintf("bad fd     -> %d (want -9)\n", (int)qwRet);

	kPrintf("dispatched %d syscalls\n", (int)(kGetSyscallCount() - qwBefore));
}

// 유저 주소공간을 하나 만들어 CR3까지 갈아타 본다. 커널 절반 공유가 깨져 있으면
// mov cr3 다음 명령어에서 죽으므로, 이 명령이 끝까지 출력되는 것 자체가 증거다
void kMmTest(const char* poParamBuff)
{
	mm_t* poMm;
	vm_area_t* poVma;
	QWORD qwPhys, qwFreeBefore, qwFreeAfter, qwPrevCR3;
	volatile QWORD* pqw;
	pte_t vqEntry[4];
	char vcHex[17];
	BOOL bPrevFlag;
	int i, iUserLevels = 0;

	// slab은 객체가 다 빠져도 빈 슬랩을 캐시에 남긴다. 그래서 mm_t와
	// vm_area_t를 처음 쓰는 순간 페이지가 두 장 늘어나는데, 그건 누수가
	// 아니다. 캐시를 먼저 덥혀 놓고 측정해야 숫자가 결정적이 된다
	poMm = kMmCreate();
	if(NULL != poMm) {
		kVmaCreate(poMm, 0x400000, 0x401000, VM_READ);
		kMmDestroy(poMm);
	}

	qwFreeBefore = kGetFreePageCount();

	poMm = kMmCreate();
	if(NULL == poMm) {
		kPrintf("kMmCreate failed\n");
		return;
	}
	kToHexString(poMm->qwPML4, vcHex, 12);
	kPrintf("mm created pml4=%s mms=%q\n", vcHex, kGetMmCount());

	// VMA: 정렬 삽입, 조회, 겹침 거절
	kVmaCreate(poMm, 0x400000, 0x401000, VM_READ | VM_WRITE);
	kVmaCreate(poMm, 0x600000, 0x602000, VM_READ | VM_EXEC);
	poVma = kVmaFind(poMm, 0x400500);
	kPrintf("vma find hit=%s miss=%s overlap=%s count=%d\n",
			((NULL != poVma) && (0x400000 == poVma->qwStart)) ? "OK" : "BAD",
			(NULL == kVmaFind(poMm, 0x3FF000)) ? "OK" : "BAD",
			(NULL == kVmaCreate(poMm, 0x400800, 0x402000, VM_READ)) ? "rejected" : "ACCEPTED",
			poMm->iVmaCount);

	qwPhys = kAllocPage();
	if(0 == qwPhys) {
		kPrintf("alloc failed\n");
		kMmDestroy(poMm);
		return;
	}
	if(FALSE == kMmMapPage(poMm, 0x400000, qwPhys, VM_READ | VM_WRITE)) {
		kPrintf("kMmMapPage failed\n");
		kFreePage(qwPhys);
		kMmDestroy(poMm);
		return;
	}

	if(PG_LEVEL_4K == kWalkPageTable(poMm->qwPML4, 0x400000, vqEntry)) {
		for(i=0; i<4; ++i) {
			if(0 != (vqEntry[i] & PTE_US)) {
				++iUserLevels;
			}
		}
	}
	kPrintf("US levels %d/4\n", iUserLevels);

	// CR3를 바꾸는 동안 선점되면 다른 태스크가 이 주소공간에서 돈다
	bPrevFlag = kSetInterruptFlag(FALSE);
	qwPrevCR3 = kReadCR3();
	kWriteCR3(poMm->qwPML4);

	pqw = (volatile QWORD*)0x400000UL;
	*pqw = 0x5EE0FF1CE0000001;

	kWriteCR3(qwPrevCR3);
	kSetInterruptFlag(bPrevFlag);

	// 유저 VA로 쓴 값이 정말 그 프레임에 들어갔는지 direct map으로 확인한다
	kPrintf("user write via CR3 switch: %s\n",
			(0x5EE0FF1CE0000001 == *(volatile QWORD*)__va(qwPhys)) ? "OK" : "MISMATCH");

	kFreePage(qwPhys);
	kMmDestroy(poMm);

	qwFreeAfter = kGetFreePageCount();
	kPrintf("mm destroyed mms=%q free before=%q after=%q %s\n",
			kGetMmCount(), qwFreeBefore, qwFreeAfter,
			(qwFreeBefore == qwFreeAfter) ? "OK" : "LEAK");
}

// cr3test가 띄우는 태스크. 0x400000은 이 태스크의 주소공간에만 매핑돼 있으므로,
// 컨텍스트 전환이 CR3를 따라오지 않으면 여기서 #PF가 나고 패닉으로 드러난다
static volatile QWORD g_qwCR3TestValue = 0;
static volatile QWORD g_qwCR3TestSeen = 0;
static volatile int g_iCR3TestDone = 0;

static void kCR3TestTask(void)
{
	g_qwCR3TestValue = *(volatile QWORD*)0x400000UL;
	g_qwCR3TestSeen = PTE_ADDR(kReadCR3());
	g_iCR3TestDone = 1;

	while(1) {
		kSchedule();
	}
}


void kCR3Test(const char* poParamBuff)
{
	mm_t* poMm;
	TCB_t* poTask;
	QWORD qwPhys, qwStartTick;
	char vcHex[17];
	BOOL bPrevFlag;

	poMm = kMmCreate();
	if(NULL == poMm) {
		kPrintf("kMmCreate failed\n");
		return;
	}

	qwPhys = kAllocPage();
	if(0 == qwPhys) {
		kPrintf("alloc failed\n");
		kMmDestroy(poMm);
		return;
	}
	*(volatile QWORD*)__va(qwPhys) = 0xC0DE1234ABCD5678;
	kVmaCreate(poMm, 0x400000, 0x401000, VM_READ | VM_WRITE);
	kMmMapPage(poMm, 0x400000, qwPhys, VM_READ | VM_WRITE);

	g_qwCR3TestValue = 0;
	g_qwCR3TestSeen = 0;
	g_iCR3TestDone = 0;

	// kCreateTask는 곧바로 ready 리스트에 넣는다. 여기서 선점되면 태스크가
	// 커널 CR3로 돌면서 0x400000을 읽어 죽으므로 바인딩까지 원자적으로 한다
	bPrevFlag = kSetInterruptFlag(FALSE);
	poTask = kCreateTask(0, (QWORD)kCR3TestTask);
	if(NULL != poTask) {
		kSetTaskMm(poTask, poMm);
	}
	kSetInterruptFlag(bPrevFlag);

	if(NULL == poTask) {
		kPrintf("kCreateTask failed\n");
		kFreePage(qwPhys);
		kMmDestroy(poMm);
		return;
	}

	qwStartTick = g_qwTickCount;
	while((0 == g_iCR3TestDone) && ((g_qwTickCount - qwStartTick) < 1000)) {
		kSchedule();
	}

	if(0 == g_iCR3TestDone) {
		kPrintf("task never ran (timeout)\n");
	}
	else {
		kToHexString(g_qwCR3TestSeen, vcHex, 12);
		kPrintf("task cr3=%s want=", vcHex);
		kToHexString(PTE_ADDR(poMm->qwPML4), vcHex, 12);
		kPrintf("%s %s\n", vcHex,
				(g_qwCR3TestSeen == PTE_ADDR(poMm->qwPML4)) ? "OK" : "BAD");
		kPrintf("task read private VA: %s\n",
				(0xC0DE1234ABCD5678 == g_qwCR3TestValue) ? "OK" : "MISMATCH");
	}

	// 커널 스레드는 CR3를 빌려 쓰므로 셸이 아직 그 주소공간 위에 있을 수 있다.
	// kMmDestroy가 그걸 알아채고 커널 CR3로 돌려놓는지 함께 본다
	kPrintf("shell borrowed mm cr3: %s\n",
			(PTE_ADDR(kReadCR3()) == PTE_ADDR(poMm->qwPML4)) ? "yes (lazy TLB)" : "no");

	kEndTask(poTask->stLink.qwID);
	kFreePage(qwPhys);
	kMmDestroy(poMm);

	kPrintf("after destroy cr3 is kernel: %s\n",
			(PTE_ADDR(kReadCR3()) == PTE_ADDR(kGetKernelCR3())) ? "OK" : "BAD");
}
