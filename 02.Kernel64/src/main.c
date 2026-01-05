/*
 * main.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "types.h"
#include "keyboard.h"
#include "descriptor.h"

void kPrintString(int x, int y, const char* str);

void main(void)
{
	char vcTmp[2] = {0,};
	BYTE ucFlag;
	BYTE ucTmp;
	int i=0;

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
	if(TRUE == kEnableKeyboard()) {
		kPrintString(37, 14, " OK ");
		kChangeKeyboardLED(FALSE, FALSE, FALSE);
	}
	else {
		kPrintString(37, 14, "Fail");
		kPrintString(0, 15, "Fail to initializing Keyboard.");
		while(1);
	}

	while(1) {
		// 출력 버퍼에 데이터가 있는 경우
		if(TRUE == kIsOutputBufferFull()) {
			// 출력 버퍼를 읽어옴
			ucTmp = kGetKeyboardScanCode();

			// 키 정보 확인
			if(TRUE == kConvertScanCodeToASCIICode(ucTmp, &(vcTmp[0]), &ucFlag)) {
				// 눌린 키를 화면에 출력
				// 문자만 출력
				if((32 <= vcTmp[0]) && (vcTmp[0] <= 126)) {
					if(ucFlag & KEY_FLAG_DOWN) {
						kPrintString(i++, 16, vcTmp);
						ucTmp /= 0;
					}
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
