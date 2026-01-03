/*
 * main.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "types.h"
#include "keyboard.h"

void kPrintString(int x, int y, const char* str);

void main(void)
{
	char vcTmp[2] = {0,};
	BYTE ucFlag;
	BYTE ucTmp;
	int i=0;

	kPrintString(0, 10, "IA-32e Mode Kernel Start............[ OK ]");
	kPrintString(0, 11, "Initializing Keyboard Interface.....[    ]");

	// 키보드 활성화
	if(TRUE == kEnableKeyboard()) {
		kPrintString(37, 11, " OK ");
		kChangeKeyboardLED(FALSE, FALSE, FALSE);
	}
	else {
		kPrintString(37, 11, "Fail");
		kPrintString(0, 12, "Fail to initializing Keyboard.");
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
				if(ucFlag & KEY_FLAG_DOWN) {
					kPrintString(i++, 12, vcTmp);
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
