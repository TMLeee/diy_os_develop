/*
 * console.h
 *
 *  Created on: 2026. 1. 25.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_CONSOLE_H_
#define __02_KERNEL64_SRC_CONSOLE_H_


#include "types.h"

// Macro
// 비디오 메모리 속성 값
#define CONSOLE_BACKGROUND_BLACK		0x00
#define CONSOLE_BACKGROUND_BLUE			0x10
#define CONSOLE_BACKGROUND_GREEN		0x20
#define CONSOLE_BACKGROUND_CYAN			0x30
#define CONSOLE_BACKGROUND_RED			0x40
#define CONSOLE_BACKGROUND_MAGENTA		0x50
#define CONSOLE_BACKGROUND_BROWN		0x60
#define CONSOLE_BACKGROUND_WHITE		0x70
#define CONSOLE_BACKGROUND_BLINK		0x80

#define CONSOLE_FOREGROUND_DARKBLACK	0x00
#define CONSOLE_FOREGROUND_DARKBLUE		0x01
#define CONSOLE_FOREGROUND_DARKGREEN	0x02
#define CONSOLE_FOREGROUND_DARKCYAN		0x03
#define CONSOLE_FOREGROUND_DARKRED		0x04
#define CONSOLE_FOREGROUND_DARKMAGENTA	0x05
#define CONSOLE_FOREGROUND_DARKBROWN	0x06
#define CONSOLE_FOREGROUND_DARKWHITE	0x07
#define CONSOLE_FOREGROUND_BRIGHBLACK	0x08
#define CONSOLE_FOREGROUND_BRIGHBLUE	0x09
#define CONSOLE_FOREGROUND_BRIGHGREEN	0x0A
#define CONSOLE_FOREGROUND_BRIGHCYAN	0x0B
#define CONSOLE_FOREGROUND_BRIGHRED		0x0C
#define CONSOLE_FOREGROUND_BRIGHMAGENTA	0x0D
#define CONSOLE_FOREGROUND_BRIGHYELLOW	0x0E
#define CONSOLE_FOREGROUND_BRIGHWHITE	0x0F

// 기본 문자 색상
#define CONSOLE_DEFAULT_TEXT_COLOR	(CONSOLE_BACKGROUND_BLACK | CONSOLE_FOREGROUND_BRIGHWHITE)

// 기본 콘솔 크기
#define CONSOLE_WIDTH			80
#define CONSOLE_HEIGHT			25
#define CONSOLE_VIDEO_MEM_ADDR	0xB8000

// 비디오 컨트롤러 IO 레지스터 주소
#define VGA_PORT_INDEX			0x3D4
#define VGA_PORT_DATA			0x3D5
#define VGA_INDEX_UPPER_CURSOR	0x0E
#define VGA_INDEX_LOWER_CURSOR	0x0F


// Struct
#pragma pack (push, 1)

typedef struct kConsoleMngStruct {
	// Cursor Positon
	int iCurPrintOfs;
}ConsoleMng_t;

#pragma pack (pop)


void kInitializeConsole(int iX, int iY);
void kSetCursor(int iX, int iY);
void kGetCursor(int* poX, int* poY);
void kPrintf(const char* format, ...);
int kConsolePrintString(const char* str);
void kCleanScreen(void);
BYTE kGetch(void);
void kPrintStringXY(int iX, int iY, const char* str);


#endif /* 02_KERNEL64_SRC_CONSOLE_H_ */
