/*
 * panic.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_PANIC_H_
#define __02_KERNEL64_SRC_PANIC_H_

#include "types.h"

// isr.asm의 KSAVECONTEXT 직후 RSP 기준 QWORD 인덱스 (0~18은 Context_t와 동일)
#define PANIC_FRAME_GS_IDX			0
#define PANIC_FRAME_FS_IDX			1
#define PANIC_FRAME_ES_IDX			2
#define PANIC_FRAME_DS_IDX			3
#define PANIC_FRAME_R15_IDX			4
#define PANIC_FRAME_R14_IDX			5
#define PANIC_FRAME_R13_IDX			6
#define PANIC_FRAME_R12_IDX			7
#define PANIC_FRAME_R11_IDX			8
#define PANIC_FRAME_R10_IDX			9
#define PANIC_FRAME_R9_IDX			10
#define PANIC_FRAME_R8_IDX			11
#define PANIC_FRAME_RSI_IDX			12
#define PANIC_FRAME_RDI_IDX			13
#define PANIC_FRAME_RDX_IDX			14
#define PANIC_FRAME_RCX_IDX			15
#define PANIC_FRAME_RBX_IDX			16
#define PANIC_FRAME_RAX_IDX			17
#define PANIC_FRAME_RBP_IDX			18

// 여기부터는 CPU가 넣은 iretq 프레임. 에러 코드가 있으면 1칸씩 밀린다
#define PANIC_FRAME_RIP_IDX			19
#define PANIC_ERRCODE_SHIFT			1

#define PANIC_EXCEPTION_CNT			32


BOOL kIsExceptionHasErrCode(int iVectorNum);
const char* kGetExceptionName(int iVectorNum);
void kDumpRegisters(QWORD* pqwFrame, int iVectorNum, QWORD qwErrCode, BOOL bHasErrCode);
void kPanic(const char* pcFmt, ...);


#endif /* 02_KERNEL64_SRC_PANIC_H_ */
