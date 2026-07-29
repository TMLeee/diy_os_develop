/*
 * task.c
 *
 *  Created on: 2026. 2. 1.
 *      Author: Macbook_pro
 */


#include "task.h"
#include "descriptor.h"
#include "utility.h"

// scheduler
static Scheduler_t gstScheduler;
static TcbPoolManager_t gstTCBPoolManager;

void kInitializeTCBPool(void)
{
	int i;

	kMemSet(&(gstTCBPoolManager), 0, sizeof(gstTCBPoolManager));

	gstTCBPoolManager.poStartAddr = (TCB_t*)TASK_TCB_POLL_ADDR;
	kMemSet((void*)TASK_TCB_POLL_ADDR, 0, sizeof(TCB_t) * TASK_MAX_CNT);

	for(i=0; i<TASK_MAX_CNT; ++i) {
		gstTCBPoolManager.poStartAddr[i].stLink.qwID = i;
	}

	gstTCBPoolManager.iMaxCnt = TASK_MAX_CNT;
	gstTCBPoolManager.iAllocatedCnt = 1;
}


TCB_t *kAllocateTCB(void)
{
	TCB_t* poEmptyTCB = NULL;
	int i;

	if(gstTCBPoolManager.iUseCnt == gstTCBPoolManager.iMaxCnt) {
		return NULL;
	}

	for(i=0; i<gstTCBPoolManager.iMaxCnt; ++i) {
		if(0 == (gstTCBPoolManager.poStartAddr[i].stLink.qwID >> 32)) {
			poEmptyTCB = &(gstTCBPoolManager.poStartAddr[i]);
			break;
		}
	}

	// 스캔이 빈손이면 초기화되지 않은 포인터에 쓰게 되므로 여기서 끊는다
	if(NULL == poEmptyTCB) {
		return NULL;
	}

	poEmptyTCB->stLink.qwID = ((QWORD)gstTCBPoolManager.iAllocatedCnt << 32) | i;
	++gstTCBPoolManager.iUseCnt;
	++gstTCBPoolManager.iAllocatedCnt;
	if(0 == gstTCBPoolManager.iAllocatedCnt) {
		gstTCBPoolManager.iAllocatedCnt = 1;
	}

	return poEmptyTCB;
}


void kFreeTCB(QWORD qwID)
{
	int i;

	i = qwID & 0xFFFFFFFF;

	kMemSet( &(gstTCBPoolManager.poStartAddr[i].tContext), 0, sizeof(Context_t));
	gstTCBPoolManager.poStartAddr[i].stLink.qwID = i;

	--gstTCBPoolManager.iUseCnt;
}


TCB_t* kCreateTask(QWORD qwFlag, QWORD qwEntryPointAddr)
{
	TCB_t* poTask;
	void* poStackAddr;

	poTask = kAllocateTCB();
	if(NULL == poTask) {
		return NULL;
	}

	poStackAddr = (void*)(TASK_STACK_POOL_ADDR + (TASK_STACK_SIZE * (poTask->stLink.qwID & 0xFFFFFFFF)));
	kSetupTask(poTask, qwFlag, qwEntryPointAddr, poStackAddr, TASK_STACK_SIZE);
	kAddTaskToReadyList(poTask);
	
	return poTask;
}


void kSetupTask(TCB_t* poTCB, QWORD qwFlag, QWORD qwEntryPointAddr, void* poStackAddr, QWORD qwStackSize)
{
	// Initialize Context
	kMemSet(poTCB->tContext.vqRegister, 0, sizeof(poTCB->tContext.vqRegister));

	// 스텍 설정
	poTCB->tContext.vqRegister[TASK_RSP_OFFSET]	= (QWORD)poStackAddr + qwStackSize;
	poTCB->tContext.vqRegister[TASK_RBP_OFFSET]	= (QWORD)poStackAddr + qwStackSize;

	// Segment 설정
	poTCB->tContext.vqRegister[TASK_CS_OFFSET] = GDT_KERNEL_CODE_SEGMENT;
	poTCB->tContext.vqRegister[TASK_DS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	poTCB->tContext.vqRegister[TASK_ES_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	poTCB->tContext.vqRegister[TASK_FS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	poTCB->tContext.vqRegister[TASK_GS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	poTCB->tContext.vqRegister[TASK_SS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;

	// RIP 레지스터, Interrupt 설정
	poTCB->tContext.vqRegister[TASK_RIP_OFFSET] = qwEntryPointAddr;

	// 인터럽트 활성화
	poTCB->tContext.vqRegister[TASK_RFLAGS_OFFSET] |= 0x0200;

	// ID, Stack, Flag 지정
	poTCB->pvStackAddr = poStackAddr;
	poTCB->qwStackSize = qwStackSize;
	poTCB->qwFlag = qwFlag;
}


void kInitializeScheduler(void)
{
	kInitializeTCBPool();
	kInitializeList(&(gstScheduler.stReadyList));
	gstScheduler.poRunningTask = kAllocateTCB();
}


void kSetRunningTask(TCB_t *poTask)
{
	gstScheduler.poRunningTask = poTask;
}


TCB_t* kGetRunningTask(void)
{
	return gstScheduler.poRunningTask;
}


TCB_t* kGetNextTaskToRun(void)
{
	if(0 == kGetListCount(&(gstScheduler.stReadyList))) {
		return NULL;
	}

	return (TCB_t*)kRemoveListFromHead(&(gstScheduler.stReadyList));
}


void kAddTaskToReadyList(TCB_t* poTask)
{
	kAddListToTail(&(gstScheduler.stReadyList), poTask);
}


void kSchedule(void)
{
	TCB_t *poRunningTask, *poNextTask;
	BOOL bPrevFlag;

	if(0 == kGetListCount(&(gstScheduler.stReadyList))) {
		return;
	}

	bPrevFlag = kSetInterruptFlag(FALSE);
	poNextTask = kGetNextTaskToRun();
	if(NULL == poNextTask) {
		kSetInterruptFlag(bPrevFlag);
		return;
	}

	poRunningTask = gstScheduler.poRunningTask;
	kAddTaskToReadyList(poRunningTask);

	gstScheduler.iProcessorTime = TASK_PROCESSOR_TIME;

	gstScheduler.poRunningTask = poNextTask;
	kSwitchContext(&(poRunningTask->tContext), &(poNextTask->tContext));

	kSetInterruptFlag(bPrevFlag);
}


BOOL kScheduleInInterrunt(void)
{
	TCB_t *poRunningTask, *poNextTask;
	char *pcContextAddr;

	poNextTask = kGetNextTaskToRun();
	if(NULL == poNextTask) {
		return FALSE;
	}

	// Switch Task
	pcContextAddr = (char*)IST_START_ADDR + IST_SIZE - sizeof(Context_t);

	poRunningTask = gstScheduler.poRunningTask;
	kMemCpy(&(poRunningTask->tContext), pcContextAddr, sizeof(Context_t));
	kAddTaskToReadyList(poRunningTask);

	gstScheduler.poRunningTask = poNextTask;
	kMemCpy(pcContextAddr, &(poNextTask->tContext), sizeof(Context_t));

	gstScheduler.iProcessorTime = TASK_PROCESSOR_TIME;
	return TRUE;
}


void kDecreaseProcessorTime(void)
{
	if(0 < gstScheduler.iProcessorTime) {
		--gstScheduler.iProcessorTime;
	}
}


BOOL kIsProcessorTimeExpired(void)
{
	if(gstScheduler.iProcessorTime <= 0) {
		return TRUE;
	}
	return FALSE;
}
