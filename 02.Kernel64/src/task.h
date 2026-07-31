/*
 * task.h
 *
 *  Created on: 2026. 2. 1.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_TASK_H_
#define __02_KERNEL64_SRC_TASK_H_


#include "types.h"
#include "list.h"


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

// 풀 크기. 주소는 더 이상 고정이 아니다 - 부팅 때 프레임 할당자에서 받는다
#define TASK_MAX_CNT			1024
#define TASK_STACK_SIZE			8192

// Invalid task id
#define TASK_INVALID_ID			0xFFFFFFFFFFFFFFFF

// max processing time(ms)
#define TASK_PROCESSOR_TIME		5


#pragma pack (push, 1)

// Contect 자료구조
typedef struct kContextStruct{
	QWORD vqRegister[TASK_REGISTER_COUNT];
}Context_t;


// 테스크 상태 관리 자료구조
typedef struct kTaskControlBlockStruct{

	// Next data position, id
	ListLink_t stLink;

	// Flag
	QWORD qwFlag;

	// Context
	Context_t tContext;

	// Stack Address, Size
	void* pvStackAddr;
	QWORD qwStackSize;
}TCB_t;


// TCB 풀 상태 관리 자료구조
typedef struct kTCBPoolManagerStruct {
	// Infomation of tack pools
	TCB_t *poStartAddr;
	QWORD qwStackPoolAddr;		// 스택 풀 베이스. 예전에는 매크로 상수였다
	int iMaxCnt;
	int iUseCnt;

	// Allocated count of TCB
	int iAllocatedCnt;
}TcbPoolManager_t;


// 스케줄러 상태 관리 자료구조
typedef struct kSchedulerStruct {
	TCB_t *poRunningTask;

	int iProcessorTime;

	List_t stReadyList;
}Scheduler_t;

#pragma pack (pop)

// Task pool functions
BOOL kInitializeTCBPool(void);
TCB_t *kAllocateTCB(void);
void kFreeTCB(QWORD qwID);
TCB_t* kCreateTask(QWORD qwFlag, QWORD qwEntryPointAddr);
void kSetupTask(TCB_t* poTCB, QWORD qwFlag, QWORD qwEntryPointAddr,
	void *poStackAddr, QWORD qwStackSize);

// Scheduler functions
BOOL kInitializeScheduler(void);
void kSetRunningTask(TCB_t *poTask);
TCB_t* kGetRunningTask(void);
TCB_t* kGetNextTaskToRun(void);
void kAddTaskToReadyList(TCB_t* poTask);
void kSchedule(void);
BOOL kScheduleInInterrunt(void);
void kDecreaseProcessorTime(void);
BOOL kIsProcessorTimeExpired(void);

#endif /* 02_KERNEL64_SRC_TASK_H_ */
