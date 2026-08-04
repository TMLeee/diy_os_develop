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
#include "mm.h"
#include "mm_struct.h"


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

#define TASK_MAX_CNT			1024

// 스스로 끝낸 태스크는 ready 리스트에 없으므로 표시가 필요하다
#define TASK_FLAG_DEAD			0x8000000000000000UL

// 리눅스 THREAD_SIZE와 같은 16KB. 스택마다 아래에 매핑하지 않은 페이지를
// 한 장 둬서 오버플로가 조용한 손상 대신 #PF가 되게 한다
#define TASK_STACK_SIZE			16384
#define TASK_STACK_PAGES		(TASK_STACK_SIZE / PAGE_SIZE)

// Invalid task id
#define TASK_INVALID_ID			0xFFFFFFFFFFFFFFFF

// max processing time(ms)
#define TASK_PROCESSOR_TIME		5

// 우선 순위별 준비 리스트의 수
#define TASK_MAX_READY_LIST_CNT	5

// 테스크 우선 순위. qwFlag의 하위 1바이트를 쓴다
#define TASK_FLAG_HIGHEST		0
#define TASK_FLAG_HIGH			1
#define TASK_FLAG_MEDIUM		2
#define TASK_FLAG_LOW			3
#define TASK_FLAG_LOWEST		4
#define TASK_FLAG_WAIT			0xFF

#define TASK_FLAG_IDLE			0x0800000000000000UL

#define GET_PRIORITY(X)				((X) & 0xFF)
#define SET_PRIORITY(X, PRIORITY)	((X) = ((X) & 0xFFFFFFFFFFFFFF00UL) | (PRIORITY))


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

	mm_t* poMM;
	QWORD qwCR3;
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

	// 우선 순위별 준비 리스트와 각 우선 순위의 실행 횟수
	List_t vstReadyList[TASK_MAX_READY_LIST_CNT];
	int viExecuteCnt[TASK_MAX_READY_LIST_CNT];

	// 회수를 기다리는 태스크
	List_t stWaitList;

	QWORD qwProcessorLoad;
	QWORD qwSpendProcessorTimeInIdleTask;
}Scheduler_t;

#pragma pack (pop)

// Task pool functions
BOOL kInitializeTCBPool(void);
TCB_t *kAllocateTCB(void);
void kFreeTCB(QWORD qwID);
TCB_t* kCreateTask(QWORD qwFlag, QWORD qwEntryPointAddr);
void kSetTaskMm(TCB_t* poTask, mm_t* poMm);
TCB_t* kCreateUserTask(mm_t* poMm, QWORD qwEntryAddr, QWORD qwUserStackTop);
BOOL kEndTask(QWORD qwTaskID);
void kExitTask(void);
TCB_t* kForkTask(mm_t* poMm, QWORD* pqwFrame);
int kEndAllUserTasks(void);
void kSetupTask(TCB_t* poTCB, QWORD qwFlag, QWORD qwEntryPointAddr,
	void *poStackAddr, QWORD qwStackSize);

// Scheduler functions
BOOL kInitializeScheduler(void);
void kSetRunningTask(TCB_t *poTask);
TCB_t* kGetRunningTask(void);
TCB_t* kGetNextTaskToRun(void);
BOOL kAddTaskToReadyList(TCB_t* poTask);
TCB_t* kRemoveTaskFromReadyList(QWORD qwTaskID);
BOOL kChangePriority(QWORD qwTaskID, BYTE ucPriority);
void kSchedule(void);
BOOL kScheduleInInterrunt(void);
void kDecreaseProcessorTime(void);
BOOL kIsProcessorTimeExpired(void);
int kGetReadyTaskCount(void);
int kGetTaskCount(void);
TCB_t* kGetTCBInTCBPool(int iOffset);
BOOL kIsTaskExist(QWORD qwID);
QWORD kGetProcessorLoad(void);

// 유휴 태스크 관련
void kIdleTask(void);
void kHaltProcessorByLoad(void);

#endif /* 02_KERNEL64_SRC_TASK_H_ */
