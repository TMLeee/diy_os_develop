/*
 * console.c
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */


#include <stdarg.h>
#include "console.h"
#include "keyboard.h"
#include "assembly_utils.h"
#include "utility.h"
#include "serial.h"

// 콘솔 정보를 관리하는 변수
ConsoleMng_t gtConsoleManager = {0,};


void kInitializeConsole(int iX, int iY)
{
	// 변수 초기화
	kMemSet(&gtConsoleManager, 0, sizeof(ConsoleMng_t));

	// 커서 위치 설정
	kSetCursor(iX, iY);
}


void kSetCursor(int iX, int iY)
{
	int iCurOfs;

	iCurOfs = iY * CONSOLE_WIDTH + iX;

	// CRTC 레지스터를 통해 커서 위치 지정
	// 상위 레지스터
	kOutPortByte(VGA_PORT_INDEX, VGA_INDEX_UPPER_CURSOR);
	kOutPortByte(VGA_PORT_DATA, (iCurOfs >> 8));

	// 하위 레지스터
	kOutPortByte(VGA_PORT_INDEX, VGA_INDEX_LOWER_CURSOR);
	kOutPortByte(VGA_PORT_DATA, iCurOfs & 0xFF);

	// 문자열 위치 업데이트
	gtConsoleManager.iCurPrintOfs = iCurOfs;
}


void kGetCursor(int* poX, int* poY)
{

	// 현재 커서 위치를 읽어옴
	*poX = gtConsoleManager.iCurPrintOfs % CONSOLE_WIDTH;
	*poY = gtConsoleManager.iCurPrintOfs / CONSOLE_WIDTH;
}


void kPrintf(const char* format, ...)
{
	va_list ap;
	char vcBuff[1024];
	int iNextPrintOfs;

	va_start(ap, format);
	kVSPrintf(vcBuff, format, ap);
	va_end(ap);

	// 화면 출력 후 출력 위치 업데이트
	iNextPrintOfs = kConsolePrintString(vcBuff);

	// 커서 위치 업데이트
	kSetCursor(iNextPrintOfs % CONSOLE_WIDTH, iNextPrintOfs / CONSOLE_WIDTH);
}


/*
 *  화면에 찍히는 문자를 시리얼 로그로 미러링
 *
 *  화면은 25줄이라 스크롤되면 지나간 내용을 잃지만 시리얼 로그는 전부 남는다.
 *  단 로그가 제어문자로 더러워지지 않도록 출력 가능한 ASCII와 개행/탭만 통과시킨다.
 *
 *  주의: 커서를 옮겨 가며 찍는 출력(kPrintf 후 kSetCursor로 되돌아가 " OK "를
 *  덧쓰는 main()의 패턴)은 화면에서는 한 줄이지만 로그에서는 두 줄로 보인다.
 *  이는 시리얼이 스트림이기 때문이며 정보 손실은 아니다.
 */
static void kMirrorCharToSerial(char cCh)
{
	// '\n'은 kSerialPutChar가 "\r\n"으로 확장한다
	if(('\n' == cCh) || ('\t' == cCh)) {
		kSerialPutChar(cCh);
	}
	else if((0x20 <= (BYTE)cCh) && ((BYTE)cCh <= 0x7E)) {
		kSerialPutChar(cCh);
	}
	// 그 외 제어문자는 로그에 남기지 않는다
}


int kConsolePrintString(const char* str)
{
	CharStruct* poScreen = (CharStruct*)CONSOLE_VIDEO_MEM_ADDR;
	int i, j;
	int iLength;
	int iPrintOfs;

	// 출력 위치 복사
	iPrintOfs = gtConsoleManager.iCurPrintOfs;

	// 문자열을 화면에 출력
	iLength = kStrLen(str);
	for(i=0; i<iLength; ++i) {

		// 시리얼 로그 미러링.
		// kPrintf를 포함한 일반 텍스트 출력은 전부 이 함수를 지나가므로
		// 여기 한 곳만 걸면 셸 전체가 로깅된다.
		// (kPrintStringXY는 의도적으로 제외 - 타이머 핸들러가 매 틱 호출하므로
		//  미러링하면 로그가 인터럽트 카운터로 뒤덮인다)
		kMirrorCharToSerial(str[i]);

		// 줄바꿈
		if('\n' == str[i]) {
			iPrintOfs += (CONSOLE_WIDTH - (iPrintOfs % CONSOLE_WIDTH));
		}
		// 탭처리
		else if('\t' == str[i]) {
			iPrintOfs += (8 - (iPrintOfs % 8));
		}
		// 일반 문자열
		else {
			poScreen[iPrintOfs].ucChar	= str[i];
			poScreen[iPrintOfs].ucAttr	= CONSOLE_DEFAULT_TEXT_COLOR;
			++iPrintOfs;
		}
	}

	// 스크롤 처리
	if((CONSOLE_HEIGHT * CONSOLE_WIDTH) <= iPrintOfs) {
		kMemCpy(
				(DWORD*)CONSOLE_VIDEO_MEM_ADDR,
				(DWORD*)(CONSOLE_VIDEO_MEM_ADDR + CONSOLE_WIDTH * sizeof(CharStruct)),
				(CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH * sizeof(CharStruct)
		);

		int iSize = CONSOLE_HEIGHT * CONSOLE_WIDTH;
		for(j=((CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH); j<iSize; ++j) {
			poScreen[j].ucChar	= ' ';
			poScreen[j].ucAttr	= CONSOLE_DEFAULT_TEXT_COLOR;
		}

		// 출력 위치 조정
		iPrintOfs = (CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH;
	}

	return iPrintOfs;
}


void kCleanScreen(void)
{
	CharStruct* poScreen = (CharStruct*)CONSOLE_VIDEO_MEM_ADDR;
	int i;
	// 화면 전체를 공백으롷 채운다
	for(i=0; i<CONSOLE_WIDTH * CONSOLE_HEIGHT; ++i) {
		poScreen[i].ucChar	= ' ';
		poScreen[i].ucAttr	= CONSOLE_DEFAULT_TEXT_COLOR;
	}

	// 커서를 홈으로 이동
	kSetCursor(0, 0);
}


BYTE kGetch(void)
{
	KeyData_t tData;

	// 키가 눌릴 때 까지 대기
	while(1) {
		while(FALSE == kGetKeyFromKeyQueue(&tData));

		// 키가 눌린 경우 ASCII로 변환
		if(tData.ucFlags & KEY_FLAG_DOWN) {
			return tData.ucASCIICode;
		}
	}
}


void kPrintStringXY(int iX, int iY, const char* str)
{
	CharStruct* poScreen = (CharStruct*)CONSOLE_VIDEO_MEM_ADDR;
	int i;

	poScreen += (iY * CONSOLE_WIDTH) + iX;
	for(i = 0; str[i] != 0; ++i) {
		poScreen[i].ucChar = str[i];
		poScreen[i].ucAttr = CONSOLE_DEFAULT_TEXT_COLOR;
	}
}
