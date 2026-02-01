/*
 * console_shell.h
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_CONSOLE_SHELL_H_
#define __02_KERNEL64_SRC_CONSOLE_SHELL_H_


#include "types.h"

#define CONSOLESHELL_MAX_COM_BUFF_SIZE	300
#define CONSOLESHELL_PROG_MSG			"TM_OS1>"

// 커멘드 함수 포인터 정의
typedef void(*poCmdFunc)(const char* strParam);


// 구조체
#pragma pack (push, 1)

// 커멘드 변수
typedef struct kShellCmdEntryStruct{
	char* strCmd;
	char* strHelp;
	poCmdFunc pfFunc;
}ShellCmdEntry_t;

// 파라미터 변수
typedef struct kParamListStruct{
	const char* poBuff;
	int iLength;
	int iCurPos;
}ParamList_t;

#pragma pack (pop)


// Shell 함수
void kStartConsoleShell(void);
void kExecuteCmd(const char* poCmdBuff);
void kInitializeParam(ParamList_t* poList, const char* poParam);
int kGetNextParam(ParamList_t* poList, char* poParam);

// Command 함수
void kHelp(const char* poParamBuff);
void kCls(const char* poParamBuff);
void kShowTotalRAMSize(const char* poParamBuff);
void kStringToDecimalHexTest(const char* poParamBuff);
void kShutdown(const char* poParamBuff);
void kSetTimer(const char* poParamBuff);
void kWaitUsingPIT(const char* poParamBuff);
void kReadTimeStampCounter(const char* poParamBuff);
void kMeasureProcessorSpeed(const char* poParamBuff);
void kShowDateAndTime(const char* poParamBuff);
void kCreateTestTask(const char* poParamBuff);


#endif /* 02_KERNEL64_SRC_CONSOLE_SHELL_H_ */
