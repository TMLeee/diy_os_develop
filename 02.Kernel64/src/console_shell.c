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


ShellCmdEntry_t gtCommandTable[] =
{
		{"help", "Show Help", kHelp},
		{"cls", "Clear Screen", kCls},
		{"totalram", "Show Total RAM Size", kShowTotalRAMSize},
		{"strtod", "String to Decimal/Hex Convert", kStringToDecimalHexTest},
		{"shutdown", "Showdown and Reboot System", kShutdown},
		{"settiner", "Set PIT Controller Counter0, ex)settimer 10[ms] 1[periodic]", kSetTimer},
		{"wait", "Wait ms Using PIT, ex)wait 100[ms]", kWaitUsingPIT},
		{"rdtsc", "Read Time Stamp Counter", kReadTimeStampCounter},
		{"cpuspeed", "Measure Processor Speed", kMeasureProcessorSpeed},
		{"date", "Show Data and Time", kShowDateAndTime},
		{"createtask", "Create Task", kCreateTestTask}
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

	// milisecond
	if(0 == kGetNextParam(&stList, poParamBuff)) {
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
	int iLength;
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


static TCB_t gtTask[2] = {0,};
static QWORD gqwStack[1024] = {0,};
void kTestTask(void) {
	int i=0;

	while(1) {
		kPrintf("[%d] kTestTask: Press any key to switch\n", i++);
		kGetch();
		kSwitchContext(&(gtTask[1].tContext), &(gtTask[0].tContext));
	}
}


void kCreateTestTask(const char* poParamBuff)
{
	KeyData_t keyData;
	int i=0;

	kSetupTask(&(gtTask[1]), 1, 0, (QWORD)kTestTask, &(gqwStack), sizeof(gqwStack));

	// q가 입력될 때 까지 수행
	while(1) {
		kPrintf("[%d] kConsoleShell: Press any key to switch\n", i++);
		if('q' == kGetch()) {
			break;
		}
		kSwitchContext(&(gtTask[0].tContext), &(gtTask[1].tContext));
	}
}
