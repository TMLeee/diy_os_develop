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

static void kSwitchAddressSpace(TCB_t* poNext);
static void kSwitchKernelStack(TCB_t* poNext);

// scheduler
static Scheduler_t gstScheduler;
static TcbPoolManager_t gstTCBPoolManager;

// 풀의 '주소'만 할당자에서 받는다. 인덱스 산술과 qwID 인코딩은 그대로 두어
// kAllocateTCB/kFreeTCB/kCreateTask의 계약이 바뀌지 않게 한다
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

	// TCB 풀은 프레임이다. direct map으로 접근한다
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


// ring3 태스크. kCreateTask와 다른 점은 세그먼트가 유저 셀렉터이고 RSP가
// 유저 스택이라는 것뿐이다. vmalloc 스택은 여기서 커널 스택 역할을 한다
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

	kSetupTask(poTask, 0, qwEntryAddr, pvKernelStack, TASK_STACK_SIZE);

	poTask->tContext.vqRegister[TASK_CS_OFFSET] = GDT_USER_CODE_SELECTOR;
	poTask->tContext.vqRegister[TASK_DS_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_ES_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_FS_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_GS_OFFSET] = GDT_USER_DATA_SELECTOR;
	poTask->tContext.vqRegister[TASK_SS_OFFSET] = GDT_USER_DATA_SELECTOR;

	// kSetupTask가 넣어 둔 커널 스택 대신 유저 스택을 쓴다
	poTask->tContext.vqRegister[TASK_RSP_OFFSET] = qwUserStackTop;
	poTask->tContext.vqRegister[TASK_RBP_OFFSET] = qwUserStackTop;

	// 리스트에 올리기 전에 주소공간을 붙여야 한다. 순서가 바뀌면 첫 스케줄에서
	// 커널 CR3로 유저 코드를 실행하러 간다
	kSetTaskMm(poTask, poMm);
	kAddTaskToReadyList(poTask);

	return poTask;
}


// fork. 부모의 시스템콜 프레임을 그대로 자식 컨텍스트로 옮긴다. int 0x80은
// 에러코드가 없어서 그 프레임이 정확히 Context_t 레이아웃이라 가능한 일이다.
// 자식은 RAX가 0이라 같은 명령 다음 줄에서 다른 값을 보고 깨어난다
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
	poChild->qwFlag			= 0;

	kSetTaskMm(poChild, poMm);
	kAddTaskToReadyList(poChild);

	return poChild;
}


// 주소공간을 가진 태스크를 전부 끝낸다. 시험이 fork로 만든 자식까지 걷어낼
// 방법이 필요해서 둔다
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

	// 유저 태스크는 자기 주소공간을 소유한다. fork가 만든 자식의 mm은 아무도
	// 들고 있지 않으므로 여기서 정리하지 않으면 새어나간다
	if(NULL != poTask->poMM) {
		kMmDestroy(poTask->poMM);
		poTask->poMM = NULL;
		poTask->qwCR3 = 0;
	}

	kFreeTCB(poTask->stLink.qwID);
}


// 자기 자신을 끝낸다. 폴트 핸들러가 iretq 복귀 지점을 여기로 바꿔서 들어온다.
// 자기 커널 스택 위에서 돌지만 그 스택을 여기서 반납할 수는 없으므로 DEAD로
// 표시만 하고, 반납은 kEndTask가 대신 한다
void kExitTask(void)
{
	TCB_t* poTask;
	TCB_t* poNext;

	kSetInterruptFlag(FALSE);

	poTask = gstScheduler.poRunningTask;
	if(NULL != poTask) {
		poTask->qwFlag |= TASK_FLAG_DEAD;
	}

	while(1) {
		poNext = kGetNextTaskToRun();
		if(NULL != poNext) {
			gstScheduler.poRunningTask = poNext;
			kSwitchAddressSpace(poNext);
			kSwitchKernelStack(poNext);
			gstScheduler.iProcessorTime = TASK_PROCESSOR_TIME;

			// 죽은 태스크의 컨텍스트에 저장한다. 다시 읽히는 일은 없다.
			// ready 리스트에 넣지 않았으므로 여기로 돌아오지 않는다
			kSwitchContext(&(poTask->tContext), &(poNext->tContext));
		}

		// 돌릴 태스크가 없으면 인터럽트를 기다린다
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
	if((NULL == poTarget) || (poTarget == gstScheduler.poRunningTask)) {
		return FALSE;
	}

	// 스스로 죽은 태스크는 ready 리스트에 없다. 그때는 바로 반납한다
	if(0 != (poTarget->qwFlag & TASK_FLAG_DEAD)) {
		kFreeTask(poTarget);
		return TRUE;
	}

	// ready 리스트에서 빼낸다
	if(NULL == kRemoveList(&(gstScheduler.stReadyList), qwTaskID)) {
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

	// 기본은 커널 스레드다. 유저 태스크는 kSetTaskMm으로 주소공간을 붙인다
	poTCB->poMM = NULL;
	poTCB->qwCR3 = 0;

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


void kSetTaskMm(TCB_t* poTask, mm_t* poMm)
{
	if(NULL == poTask) {
		return;
	}
	poTask->poMM = poMm;
	poTask->qwCR3 = (NULL != poMm) ? poMm->qwPML4 : 0;
}


// 커널 스레드(qwCR3==0)는 현재 주소공간을 그대로 빌려 쓴다. 커널 절반이 모든
// 주소공간에서 동일하므로 안전하고, 전환마다 TLB를 비우지 않아도 된다
// int 0x80은 IST를 쓰지 않으므로 ring3에서 들어오면 CPU가 TSS.rsp0를 집는다.
// 태스크마다 커널 스택이 다르니 전환할 때마다 갱신해야 한다.
// 커널 스레드는 스택이 없거나(부팅 스택) ring3로 내려갈 일이 없어 건너뛴다
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
	// IST 스택은 direct map으로 만진다. TSS에 넣은 값과 같은 별칭이어야 한다
	pcContextAddr = (char*)__va(IST_START_ADDR + IST_SIZE) - sizeof(Context_t);

	poRunningTask = gstScheduler.poRunningTask;
	kMemCpy(&(poRunningTask->tContext), pcContextAddr, sizeof(Context_t));
	kAddTaskToReadyList(poRunningTask);

	gstScheduler.poRunningTask = poNextTask;
	kMemCpy(pcContextAddr, &(poNextTask->tContext), sizeof(Context_t));

	// IST 스택과 복귀 경로는 커널 절반에 있으므로 여기서 CR3를 바꿔도 된다
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
