/*
 * types.h
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_TYPES_H_
#define __02_KERNEL64_SRC_TYPES_H_

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

#endif /* 02_KERNEL64_SRC_TYPES_H_ */
