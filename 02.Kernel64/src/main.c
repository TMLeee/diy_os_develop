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

void kPrintString(int x, int y, const char* str);

void main(void)
{
	char vcTmp[2] = {0,};
	BYTE ucFlag;
	BYTE ucTmp;
	int i=0;
	KeyData_t tKeyData;

	kPrintString(0, 10, "IA-32e Mode Kernel Start ...........[ OK ]");
	kPrintString(0, 11, "Initializing GDT....................[    ]");
	kInitGDTTableAndTSS();
	kLoadGDTR(GDTR_START_ADDR);
	kPrintString(37, 11, " OK ");

	kPrintString(0, 12, "Initializing TSS Segment ...........[    ]");
	kLoadTR(GDT_TSS_SEGMENT);
	kPrintString(37, 12, " OK ");


	kPrintString(0, 13, "Initializing IDT ...................[    ]");
	kInitTDTTable();
	kLoadIDTR(IDTR_START_ADDR);
	kPrintString(37, 13, " OK ");

	// 키보드 활성화
	kPrintString(0, 14, "Initializing Keyboard Interface ....[    ]");
	if(TRUE == kInitializeKeyboard()) {
		kPrintString(37, 14, " OK ");
		kChangeKeyboardLED(FALSE, FALSE, FALSE);
	}
	else {
		kPrintString(37, 14, "Fail");
		kPrintString(0, 15, "Fail to initializing Keyboard.");
		while(1);
	}

	// PIC Controller 초기화
	kPrintString(0, 15, "Initializing PIC Controller ........[    ]");
	kInitializePIC();
	kMaskPICInterrupt(0);
	kEnableInterrupt();
	kPrintString(37, 15, " OK ");

	while(1) {
		// 키 큐 확인
		if(TRUE == kGetKeyFromKeyQueue(&tKeyData)) {
			if(tKeyData.ucFlags & KEY_FLAG_DOWN) {
				vcTmp[0] = tKeyData.ucASCIICode;
				kPrintString(++i, 16, vcTmp);

				// Divide 예외를 발생시킴
				if('0' == vcTmp[0]) {
					BYTE ucTmp = 1;
					ucTmp = ucTmp / 0;
				}
			}
		}
	}
}


void kPrintString(int x, int y, const char* str)
{
	CharStruct* poScreen = (CharStruct*) 0xB8000;
	int i;

	poScreen += (y * 80) + x;
	for(i = 0; str[i] != 0; ++i) {
		poScreen[i].ucChar = str[i];
	}
}
