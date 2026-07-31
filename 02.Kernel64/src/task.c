/*
 * task.c
 *
 *  Created on: 2026. 2. 1.
 *      Author: Macbook_pro
 */


#include "task.h"
#include "descriptor.h"
#include "utility.h"
#include "pmm.h"
#include "mm.h"
#include "vmalloc.h"

// scheduler
static Scheduler_t gstScheduler;
static TcbPoolManager_t gstTCBPoolManager;

// 풀의 '주소'만 할당자에서 받는다. 인덱스 산술과 qwID 인코딩은 그대로 두어
// kAllocateTCB/kFreeTCB/kCreateTask의 계약이 바뀌지 않게 한다
BOOL kInitializeTCBPool(void)
{
	QWORD qwTCBSize = ALIGN_UP(sizeof(TCB_t) * TASK_MAX_CNT, PAGE_SIZE);
	QWORD qwTCBAddr;
	void* pvStackPool;
	int i, iTCBOrder = 0;

	kMemSet(&(gstTCBPoolManager), 0, sizeof(gstTCBPoolManager));

	while(((1UL << iTCBOrder) * PAGE_SIZE) < qwTCBSize) {
		++iTCBOrder;
	}
	if(PMM_MAX_ORDER <= iTCBOrder) {
		return FALSE;
	}

	qwTCBAddr = kAllocPages(iTCBOrder);
	if(0 == qwTCBAddr) {
		return FALSE;
	}

	// 스택 풀 8MB는 order 11이라 kAllocPages의 상한(4MB)을 넘는다.
	// vmalloc은 물리적으로 흩어진 프레임을 연속 가상주소로 묶어 주므로
	// 인덱스 산술을 그대로 두면서 그 제약을 피할 수 있고, 풀 앞뒤로
	// guard page까지 덤으로 얻는다
	pvStackPool = kVmapPages((int)(TASK_MAX_CNT * TASK_STACK_SIZE / PAGE_SIZE), 1, 1);
	if(NULL == pvStackPool) {
		kFreePages(qwTCBAddr, iTCBOrder);
		return FALSE;
	}

	gstTCBPoolManager.poStartAddr = (TCB_t*)qwTCBAddr;
	gstTCBPoolManager.qwStackPoolAddr = (QWORD)pvStackPool;
	kMemSet((void*)qwTCBAddr, 0, (int)(sizeof(TCB_t) * TASK_MAX_CNT));

	for(i=0; i<TASK_MAX_CNT; ++i) {
		gstTCBPoolManager.poStartAddr[i].stLink.qwID = i;
	}

	gstTCBPoolManager.iMaxCnt = TASK_MAX_CNT;
	gstTCBPoolManager.iAllocatedCnt = 1;
	return TRUE;
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

	// 인덱스 기반 산술은 그대로. 베이스만 매크로에서 변수로 바뀌었다
	poStackAddr = (void*)(gstTCBPoolManager.qwStackPoolAddr +
			((QWORD)TASK_STACK_SIZE * (poTask->stLink.qwID & 0xFFFFFFFF)));
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


BOOL kInitializeScheduler(void)
{
	if(FALSE == kInitializeTCBPool()) {
		return FALSE;
	}
	kInitializeList(&(gstScheduler.stReadyList));
	gstScheduler.poRunningTask = kAllocateTCB();
	return (NULL != gstScheduler.poRunningTask) ? TRUE : FALSE;
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
