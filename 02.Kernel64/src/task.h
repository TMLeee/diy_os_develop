/*
 * task.h
 *
 *  Created on: 2026. 2. 1.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_TASK_H_
#define __02_KERNEL64_SRC_TASK_H_


#include "types.h"


#define TASK_REGISTER_COUNT		(5 + 19)
#define TASK_REGISTER_SIZE		8

// Context 자료구조의 레지스터 오프셋
#define TASK_GS_OFFSET			0
#define TASK_FS_OFFSET			1
#define TASK_ES_OFFSET			2
#define TASK_DS_OFFSET			3
#define TASK_R15_OFFSET			4
#define TASK_R14_OFFSET			5
#define TASK_R13_OFFSET			6
#define TASK_R12_OFFSET			7
#define TASK_R11_OFFSET			8
#define TASK_R10_OFFSET			9
#define TASK_R9_OFFSET			10
#define TASK_R8_OFFSET			11
#define TASK_RSI_OFFSET			12
#define TASK_RDI_OFFSET			13
#define TASK_RDX_OFFSET			14
#define TASK_RCX_OFFSET			15
#define TASK_RBX_OFFSET			16
#define TASK_RAX_OFFSET			17
#define TASK_RBP_OFFSET			18
#define TASK_RIP_OFFSET			19
#define TASK_CS_OFFSET			20
#define TASK_RFLAGS_OFFSET		21
#define TASK_RSP_OFFSET			22
#define TASK_SS_OFFSET			23


#pragma pack (push, 1)

// Contect 자료구조
typedef struct kContextStruct{
	QWORD vqRegister[TASK_REGISTER_COUNT];
}Context_t;


// 테스크 상태 관리 자료구조
typedef struct kTaskControlBlockStruct{
	// Context
	Context_t tContext;

	// ID, Flag
	QWORD qwID;
	QWORD qwFlag;

	// Stack Address, Size
	void* pvStackAddr;
	QWORD qwStackSize;
}TCB_t;

#pragma pack (pop)

void kSetupTask(TCB_t* poTCB, QWORD qwID, QWORD qwFlag, QWORD qwEntryPointAddr, void* poStackAddr, QWORD qwStackSize);

#endif /* 02_KERNEL64_SRC_TASK_H_ */
