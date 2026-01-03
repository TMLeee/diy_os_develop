/*
 * types.h
 *
 *  Created on: 2025. 12. 31.
 *      Author: Seungmin Lee
 */

#ifndef __01_KERNEL32_SRC_TYPES_H_
#define __01_KERNEL32_SRC_TYPES_H_

#define BYTE	unsigned char
#define WORD	unsigned short
#define DWORD	unsigned int
#define QWORD	unsigned long
#define BOOL	unsigned char


#define TRUE	1
#define FALSE	0
#define NULL	0

#pragma  pack (push, 1)

// 비디오 모드에서 텍스트를 출력하기 위한 자료 구조
typedef struct kCharStruct {
	BYTE ucChar;
	BYTE ucAttr;
} CharStruct;

#pragma pack (pop)

#endif /* 01_KERNEL32_SRC_TYPES_H_ */
