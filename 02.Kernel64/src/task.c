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
#include "paging.h"
#include "assembly_utils.h"
#include "console.h"

static void kSwitchAddressSpace(TCB_t* poNext);
static void kSwitchKernelStack(TCB_t* poNext);

// scheduler
static Scheduler_t gstScheduler;
static TcbPoolManager_t gstTCBPoolManager;

BOOL kInitializeTCBPool(void)
{
	QWORD qwTCBSize = ALIGN_UP(sizeof(TCB_t) * TASK_MAX_CNT, PAGE_SIZE);
	QWORD qwTCBAddr;
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

	gstTCBPoolManager.poStartAddr = (TCB_t*)__va(qwTCBAddr);
	kMemSet(__va(qwTCBAddr), 0, (int)(sizeof(TCB_t) * TASK_MAX_CNT));

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

	// 스택마다 개별 할당. 아래에 guard page 한 장을 두면 오버플로가
	// 아래 스택을 조용히 덮는 대신 #PF로 잡힌다
	poStackAddr = kVmapPages(TASK_STACK_PAGES, 1, 0);
	if(NULL == poStackAddr) {
		kFreeTCB(poTask->stLink.qwID);
		return NULL;
	}

	kSetupTask(poTask, qwFlag, qwEntryPointAddr, poStackAddr, TASK_STACK_SIZE);
	kAddTaskToReadyList(poTask);

	return poTask;
}


// ring3 태스크. vmalloc 스택이 여기서는 커널 스택이다
TCB_t* kCreateUserTask(mm_t* poMm, QWORD qwEntryAddr, QWORD qwUserStackTop)
{
	TCB_t* poTask;
	void* pvKernelStack;

	if(NULL == poMm) {
		return NULL;
	}

	poTask = kAllocateTCB();
	if(NULL == poTask) {
		return NULL;
	}

	pvKernelStack = kVmapPages(TASK_STACK_PAGES, 1, 0);
	if(NULL == pvKernelStack) {
		kFreeTCB(poTask->stLink.qwID);
		return NULL;
	}

	kSetupTask(poTask, TASK_FLAG_MEDIUM, qwEntryAddr, pvKernelStack, TASK_STACK_SIZE);

	poTask->tContext.vqRegister[TASK_CS_OFFSET] = GDT_USER_CODE_SELECTOR;
	poTask->tContext.vqRegister[TASK_DS_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_ES_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_FS_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_GS_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_SS_OFFSET] = GDT_USER_DATA_SELECTOR;

	poTask->tContext.vqRegister[TASK_RSP_OFFSET] = qwUserStackTop;
	poTask->tContext.vqRegister[TASK_RBP_OFFSET] = qwUserStackTop;

	// 리스트에 올리기 전에 붙여야 한다. 아니면 첫 스케줄이 커널 CR3로 간다
	kSetTaskMm(poTask, poMm);
	kAddTaskToReadyList(poTask);

	return poTask;
}


// 부모의 시스템콜 프레임을 자식 컨텍스트로 옮긴다. int 0x80은 에러코드가
// 없어 그 프레임이 정확히 Context_t다. 자식만 RAX가 0이다
TCB_t* kForkTask(mm_t* poMm, QWORD* pqwFrame)
{
	TCB_t* poChild;
	void* pvKernelStack;

	if((NULL == poMm) || (NULL == pqwFrame)) {
		return NULL;
	}

	poChild = kAllocateTCB();
	if(NULL == poChild) {
		return NULL;
	}

	pvKernelStack = kVmapPages(TASK_STACK_PAGES, 1, 0);
	if(NULL == pvKernelStack) {
		kFreeTCB(poChild->stLink.qwID);
		return NULL;
	}

	kMemCpy(&(poChild->tContext), pqwFrame, sizeof(Context_t));
	poChild->tContext.vqRegister[TASK_RAX_OFFSET] = 0;

	poChild->pvStackAddr	= pvKernelStack;
	poChild->qwStackSize	= TASK_STACK_SIZE;
	poChild->qwFlag			= TASK_FLAG_MEDIUM;

	kSetTaskMm(poChild, poMm);
	kAddTaskToReadyList(poChild);

	return poChild;
}


// 주소공간을 가진 태스크를 전부 끝낸다. fork가 만든 자식까지 걷어내려고 둔다
int kEndAllUserTasks(void)
{
	TCB_t* poTask;
	int i, iEnded = 0;

	for(i=0; i<gstTCBPoolManager.iMaxCnt; ++i) {
		poTask = &(gstTCBPoolManager.poStartAddr[i]);
		if((NULL == poTask->poMM) || (poTask == gstScheduler.poRunningTask)) {
			continue;
		}
		if(TRUE == kEndTask(poTask->stLink.qwID)) {
			++iEnded;
		}
	}
	return iEnded;
}


// 태스크를 정리한다. 이 커널이 스택을 반납하는 최초의 경로
void kFreeTask(TCB_t* poTask)
{
	if((NULL == poTask) || (NULL == poTask->pvStackAddr)) {
		return;
	}

	kVfree(poTask->pvStackAddr);
	poTask->pvStackAddr = NULL;
	poTask->qwStackSize = 0;

	// 유저 태스크가 자기 주소공간을 소유한다. fork한 자식의 mm은 여기서만 정리된다
	if(NULL != poTask->poMM) {
		kMmDestroy(poTask->poMM);
		poTask->poMM = NULL;
		poTask->qwCR3 = 0;
	}

	kFreeTCB(poTask->stLink.qwID);
}


// 폴트 핸들러가 복귀 지점을 여기로 바꿔서 들어온다.
// 자기 스택 위에서 도니 반납은 못 한다. DEAD만 찍고 kEndTask가 치운다
void kExitTask(void)
{
	TCB_t* poTask;
	TCB_t* poNext;

	kSetInterruptFlag(FALSE);

	poTask = gstScheduler.poRunningTask;
	if(NULL != poTask) {
		poTask->qwFlag |= TASK_FLAG_DEAD;
		SET_PRIORITY(poTask->qwFlag, TASK_FLAG_WAIT);
	}

	while(1) {
		poNext = kGetNextTaskToRun();
		if(NULL != poNext) {
			// 주소공간이 없는 커널 스레드만 유휴 태스크가 회수한다. 주소공간을
			// 가진 유저 태스크는 예전대로 kEndTask/kEndAllUserTasks가 걷어낸다
			if((NULL != poTask) && (NULL == poTask->poMM)) {
				kAddListToTail(&(gstScheduler.stWaitList), poTask);
			}

			gstScheduler.poRunningTask = poNext;
			kSwitchAddressSpace(poNext);
			kSwitchKernelStack(poNext);
			gstScheduler.iProcessorTime = TASK_PROCESSOR_TIME;

			// ready 리스트에 없으므로 여기로 돌아오지 않는다
			kSwitchContext(NULL, &(poNext->tContext));
		}

		kSetInterruptFlag(TRUE);
		kHlt();
		kSetInterruptFlag(FALSE);
	}
}


// 현재 태스크를 ready 리스트에서 제외하고 다음으로 넘어간다
BOOL kEndTask(QWORD qwTaskID)
{
	TCB_t* poTarget = NULL;
	KListHead_t* poDummy;
	int i;

	(void)poDummy;

	for(i=0; i<gstTCBPoolManager.iMaxCnt; ++i) {
		if(gstTCBPoolManager.poStartAddr[i].stLink.qwID == qwTaskID) {
			poTarget = &(gstTCBPoolManager.poStartAddr[i]);
			break;
		}
	}
	if(NULL == poTarget) {
		return FALSE;
	}

	// 자기 자신을 끝내는 경우. 자기 스택 위에서는 반납할 수 없으니
	// DEAD만 찍고 넘어가면 유휴 태스크가 대기 리스트에서 회수한다
	if(poTarget == gstScheduler.poRunningTask) {
		kExitTask();
		return TRUE;
	}

	// 스스로 죽은 태스크는 ready 리스트에 없다
	if(0 != (poTarget->qwFlag & TASK_FLAG_DEAD)) {
		kRemoveList(&(gstScheduler.stWaitList), qwTaskID);
		kFreeTask(poTarget);
		return TRUE;
	}

	// 준비 리스트에서 빼낸다
	if(NULL == kRemoveTaskFromReadyList(qwTaskID)) {
		return FALSE;
	}

	kFreeTask(poTarget);
	return TRUE;
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

	poTCB->poMM = NULL;
	poTCB->qwCR3 = 0;

	// ID, Stack, Flag 지정
	poTCB->pvStackAddr = poStackAddr;
	poTCB->qwStackSize = qwStackSize;
	poTCB->qwFlag = qwFlag;
}


BOOL kInitializeScheduler(void)
{
	int i;

	if(FALSE == kInitializeTCBPool()) {
		return FALSE;
	}

	for(i=0; i<TASK_MAX_READY_LIST_CNT; ++i) {
		kInitializeList(&(gstScheduler.vstReadyList[i]));
		gstScheduler.viExecuteCnt[i] = 0;
	}
	kInitializeList(&(gstScheduler.stWaitList));

	gstScheduler.poRunningTask = kAllocateTCB();
	if(NULL == gstScheduler.poRunningTask) {
		return FALSE;
	}

	// 부팅을 이어받는 셸이 가장 높은 우선 순위를 갖는다
	gstScheduler.poRunningTask->qwFlag = TASK_FLAG_HIGHEST;
	gstScheduler.iProcessorTime = TASK_PROCESSOR_TIME;
	gstScheduler.qwProcessorLoad = 0;
	gstScheduler.qwSpendProcessorTimeInIdleTask = 0;

	return TRUE;
}


void kSetRunningTask(TCB_t *poTask)
{
	gstScheduler.poRunningTask = poTask;
}


TCB_t* kGetRunningTask(void)
{
	return gstScheduler.poRunningTask;
}


// 큐에 태스크가 있어도 모든 큐가 한 바퀴씩 돌아 양보만 하고 끝날 수 있어
// 한 번 더 훑는다
TCB_t* kGetNextTaskToRun(void)
{
	TCB_t* poTarget = NULL;
	int iTaskCnt;
	int i, j;

	for(j=0; j<2; ++j) {
		for(i=0; i<TASK_MAX_READY_LIST_CNT; ++i) {
			iTaskCnt = kGetListCount(&(gstScheduler.vstReadyList[i]));

			// 실행한 횟수보다 대기 중인 태스크가 많으면 이 우선 순위에서 고른다
			if(gstScheduler.viExecuteCnt[i] < iTaskCnt) {
				poTarget = (TCB_t*)kRemoveListFromHead(&(gstScheduler.vstReadyList[i]));
				++(gstScheduler.viExecuteCnt[i]);
				break;
			}

			// 다 돌았으면 횟수를 접고 다음 우선 순위로 양보한다
			gstScheduler.viExecuteCnt[i] = 0;
		}

		if(NULL != poTarget) {
			break;
		}
	}

	return poTarget;
}


BOOL kAddTaskToReadyList(TCB_t* poTask)
{
	BYTE ucPriority;

	ucPriority = GET_PRIORITY(poTask->qwFlag);
	if(TASK_MAX_READY_LIST_CNT <= ucPriority) {
		return FALSE;
	}

	kAddListToTail(&(gstScheduler.vstReadyList[ucPriority]), poTask);
	return TRUE;
}


TCB_t* kRemoveTaskFromReadyList(QWORD qwTaskID)
{
	TCB_t* poTarget;
	BYTE ucPriority;

	poTarget = kGetTCBInTCBPool((int)(qwTaskID & 0xFFFFFFFF));
	if((NULL == poTarget) || (poTarget->stLink.qwID != qwTaskID)) {
		return NULL;
	}

	ucPriority = GET_PRIORITY(poTarget->qwFlag);
	if(TASK_MAX_READY_LIST_CNT <= ucPriority) {
		return NULL;
	}

	return (TCB_t*)kRemoveList(&(gstScheduler.vstReadyList[ucPriority]), qwTaskID);
}


BOOL kChangePriority(QWORD qwTaskID, BYTE ucPriority)
{
	TCB_t* poTarget;
	BOOL bPrevFlag;

	if(TASK_MAX_READY_LIST_CNT <= ucPriority) {
		return FALSE;
	}

	bPrevFlag = kSetInterruptFlag(FALSE);

	// 실행 중인 태스크는 값만 바꾼다. 다음 전환에서 바뀐 리스트로 들어간다
	poTarget = gstScheduler.poRunningTask;
	if(poTarget->stLink.qwID == qwTaskID) {
		SET_PRIORITY(poTarget->qwFlag, ucPriority);
		kSetInterruptFlag(bPrevFlag);
		return TRUE;
	}

	// 준비 리스트에 없으면 TCB만 찾아서 값을 바꾼다
	poTarget = kRemoveTaskFromReadyList(qwTaskID);
	if(NULL == poTarget) {
		poTarget = kGetTCBInTCBPool((int)(qwTaskID & 0xFFFFFFFF));
		if((NULL == poTarget) || (poTarget->stLink.qwID != qwTaskID)) {
			kSetInterruptFlag(bPrevFlag);
			return FALSE;
		}
		SET_PRIORITY(poTarget->qwFlag, ucPriority);
		kSetInterruptFlag(bPrevFlag);
		return TRUE;
	}

	SET_PRIORITY(poTarget->qwFlag, ucPriority);
	kAddTaskToReadyList(poTarget);
	kSetInterruptFlag(bPrevFlag);
	return TRUE;
}


int kGetReadyTaskCount(void)
{
	int iTotalCnt = 0;
	int i;

	for(i=0; i<TASK_MAX_READY_LIST_CNT; ++i) {
		iTotalCnt += kGetListCount(&(gstScheduler.vstReadyList[i]));
	}

	return iTotalCnt;
}


int kGetTaskCount(void)
{
	return kGetReadyTaskCount() + kGetListCount(&(gstScheduler.stWaitList)) + 1;
}


TCB_t* kGetTCBInTCBPool(int iOffset)
{
	if((iOffset < 0) || (gstTCBPoolManager.iMaxCnt <= iOffset)) {
		return NULL;
	}

	return &(gstTCBPoolManager.poStartAddr[iOffset]);
}


BOOL kIsTaskExist(QWORD qwID)
{
	TCB_t* poTCB;

	poTCB = kGetTCBInTCBPool((int)(qwID & 0xFFFFFFFF));
	if((NULL == poTCB) || (poTCB->stLink.qwID != qwID)) {
		return FALSE;
	}
	return TRUE;
}


QWORD kGetProcessorLoad(void)
{
	return gstScheduler.qwProcessorLoad;
}


void kSetTaskMm(TCB_t* poTask, mm_t* poMm)
{
	if(NULL == poTask) {
		return;
	}
	poTask->poMM = poMm;
	poTask->qwCR3 = (NULL != poMm) ? poMm->qwPML4 : 0;
}


// 커널 스레드(qwCR3==0)는 현재 주소공간을 빌려 쓴다(lazy TLB)
// int 0x80은 IST0이라 ring3에서 들어오면 CPU가 TSS.rsp0를 집는다
static void kSwitchKernelStack(TCB_t* poNext)
{
	if(NULL != poNext->pvStackAddr) {
		kSetTSSRsp0((QWORD)poNext->pvStackAddr + poNext->qwStackSize);
	}
}


static void kSwitchAddressSpace(TCB_t* poNext)
{
	if(0 == poNext->qwCR3) {
		return;
	}
	if(PTE_ADDR(poNext->qwCR3) != PTE_ADDR(kReadCR3())) {
		kWriteCR3(poNext->qwCR3);
	}
}


void kSchedule(void)
{
	TCB_t *poRunningTask, *poNextTask;
	BOOL bPrevFlag;

	if(kGetReadyTaskCount() < 1) {
		return;
	}

	bPrevFlag = kSetInterruptFlag(FALSE);
	poNextTask = kGetNextTaskToRun();
	if(NULL == poNextTask) {
		kSetInterruptFlag(bPrevFlag);
		return;
	}

	poRunningTask = gstScheduler.poRunningTask;

	// 유휴 태스크에서 넘어왔다면 쓴 만큼을 부하 계산에 누적한다
	if(TASK_FLAG_IDLE == (poRunningTask->qwFlag & TASK_FLAG_IDLE)) {
		gstScheduler.qwSpendProcessorTimeInIdleTask +=
				TASK_PROCESSOR_TIME - gstScheduler.iProcessorTime;
	}

	kAddTaskToReadyList(poRunningTask);

	gstScheduler.iProcessorTime = TASK_PROCESSOR_TIME;

	gstScheduler.poRunningTask = poNextTask;
	kSwitchAddressSpace(poNextTask);
	kSwitchKernelStack(poNextTask);
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
	pcContextAddr = (char*)__va(IST_START_ADDR + IST_SIZE) - sizeof(Context_t);

	poRunningTask = gstScheduler.poRunningTask;

	// 유휴 태스크에서 넘어왔다면 쓴 만큼을 부하 계산에 누적한다
	if(TASK_FLAG_IDLE == (poRunningTask->qwFlag & TASK_FLAG_IDLE)) {
		gstScheduler.qwSpendProcessorTimeInIdleTask += TASK_PROCESSOR_TIME;
	}

	kMemCpy(&(poRunningTask->tContext), pcContextAddr, sizeof(Context_t));
	kAddTaskToReadyList(poRunningTask);

	gstScheduler.poRunningTask = poNextTask;
	kMemCpy(pcContextAddr, &(poNextTask->tContext), sizeof(Context_t));

	kSwitchAddressSpace(poNextTask);
	kSwitchKernelStack(poNextTask);

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


// 대기 리스트에 쌓인 태스크를 회수하고 남는 시간에 프로세서를 쉬게 한다.
// 남의 스택 위에서 도니 여기서는 스택까지 반납할 수 있다
void kIdleTask(void)
{
	TCB_t* poTask;
	QWORD qwLastMeasureTickCnt, qwLastSpendTickInIdleTask;
	QWORD qwCurMeasureTickCnt, qwCurSpendTickInIdleTask;
	BOOL bPrevFlag;

	qwLastSpendTickInIdleTask = gstScheduler.qwSpendProcessorTimeInIdleTask;
	qwLastMeasureTickCnt = kGetTickCnt();

	while(1) {
		qwCurMeasureTickCnt = kGetTickCnt();
		qwCurSpendTickInIdleTask = gstScheduler.qwSpendProcessorTimeInIdleTask;

		// 100 - (유휴 태스크가 쓴 시간 * 100 / 전체 시간)
		if(qwCurMeasureTickCnt == qwLastMeasureTickCnt) {
			gstScheduler.qwProcessorLoad = 0;
		}
		else {
			gstScheduler.qwProcessorLoad = 100 -
					((qwCurSpendTickInIdleTask - qwLastSpendTickInIdleTask) * 100 /
					 (qwCurMeasureTickCnt - qwLastMeasureTickCnt));
		}

		qwLastMeasureTickCnt = qwCurMeasureTickCnt;
		qwLastSpendTickInIdleTask = qwCurSpendTickInIdleTask;

		kHaltProcessorByLoad();

		while(0 < kGetListCount(&(gstScheduler.stWaitList))) {
			bPrevFlag = kSetInterruptFlag(FALSE);
			poTask = (TCB_t*)kRemoveListFromHead(&(gstScheduler.stWaitList));
			kSetInterruptFlag(bPrevFlag);

			if(NULL == poTask) {
				break;
			}

			kPrintf("IDLE: Task ID[0x%q] is completely ended.\n", poTask->stLink.qwID);
			kFreeTask(poTask);
		}

		kSchedule();
	}
}


void kHaltProcessorByLoad(void)
{
	if(gstScheduler.qwProcessorLoad < 40) {
		kHlt();
		kHlt();
		kHlt();
	}
	else if(gstScheduler.qwProcessorLoad < 80) {
		kHlt();
		kHlt();
	}
	else if(gstScheduler.qwProcessorLoad < 95) {
		kHlt();
	}
}
