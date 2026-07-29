/*
 * main.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "types.h"
#include "keyboard.h"
#include "descriptor.h"
#include "pic.h"
#include "console.h"
#include "console_shell.h"
#include "task.h"
#include "pic.h"
#include "serial.h"


void kPrintString(int x, int y, const char* str)
{
	CharStruct* poScreen = (CharStruct*) 0xB8000;
	int i;

	poScreen += (y * 80) + x;
	for(i = 0; str[i] != 0; ++i) {
		poScreen[i].ucChar = str[i];
	}
}


void main(void)
{
	char vcTmp[2] = {0,};
	BYTE ucFlag;
	BYTE ucTmp;
	int i=0;
	KeyData_t tKeyData;
	int iCursorX, iCursorY;

	// 시리얼(COM1)을 가장 먼저 초기화한다.
	// 콘솔/GDT/IDT 초기화 도중에 죽더라도 로그가 남아야 하므로 반드시 최초 실행.
	kInitializeSerial();
	kSerialPutString("\n[SERIAL] COM1 115200 8N1 Ready - Kernel64 Boot\n");

	kInitializeConsole(0, 10);
	kPrintf("IA-32e Mode Kernel Start ...........[ OK ]\n");
	kPrintf("Initialize Console .................[ OK ]\n");

	kGetCursor(&iCursorX, &iCursorY);
	kPrintf("Initializing GDT....................[    ]\n");
	kInitGDTTableAndTSS();
	kLoadGDTR(GDTR_START_ADDR);
	kSetCursor(37, iCursorY++);
	kPrintf(" OK \n");

	kPrintf("Initializing TSS Segment ...........[    ]\n");
	kLoadTR(GDT_TSS_SEGMENT);
	kSetCursor(37, iCursorY++);
	kPrintf(" OK \n");


	kPrintf("Initializing IDT ...................[    ]\n");
	kInitTDTTable();
	kLoadIDTR(IDTR_START_ADDR);
	kSetCursor(37, iCursorY++);
	kPrintf(" OK \n");

	kPrintf("Check System RAM Size...............[    ]\n");
	kCheckTotalRAMSize();
	kSetCursor(37, iCursorY++);
	kPrintf(" OK \n");

	kPrintf("CTCB Pool And Scheduler Initialize..[    ]\n");
	kInitializeScheduler();
	kSetCursor(38, iCursorY++);
	kPrintf(" OK \n");

	// 키보드 활성화
	kPrintf("Initializing Keyboard Interface ....[    ]\n");
	if(TRUE == kInitializeKeyboard()) {
		kSetCursor(39, iCursorY++);
		kPrintf(" OK \n");
		kChangeKeyboardLED(FALSE, FALSE, FALSE);
	}
	else {
		kSetCursor(38, iCursorY++);
		kPrintf("Fail\n");
		kPrintf("Fail to initializing Keyboard.");
		while(1);
	}

	// PIC Controller 초기화
	kPrintf("Initializing PIC Controller ........[    ]\n");
	kInitializePIC();
	kMaskPICInterrupt(0);
	kEnableInterrupt();
	kSetCursor(38, iCursorY++);
	kPrintf(" OK \n");

	kStartConsoleShell();
}
