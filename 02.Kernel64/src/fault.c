/*
 * fault.c
 *
 *  Created on: 2026. 8. 2.
 *      Author: Macbook_pro
 */


#include "fault.h"
#include "mm_struct.h"
#include "task.h"
#include "descriptor.h"
#include "console.h"
#include "utility.h"
#include "pmm.h"
#include "paging.h"
#include "assembly_utils.h"


// 에러코드 예외는 RIP부터 한 칸 밀린다. ds/es/fs/gs는 프레임 바닥이라 그대로다
#define PF_RIP_OFFSET		(TASK_RIP_OFFSET + 1)
#define PF_CS_OFFSET		(TASK_CS_OFFSET + 1)
#define PF_RFLAGS_OFFSET	(TASK_RFLAGS_OFFSET + 1)
#define PF_RSP_OFFSET		(TASK_RSP_OFFSET + 1)
#define PF_SS_OFFSET		(TASK_SS_OFFSET + 1)

#define RFLAGS_IF			0x0200


static QWORD g_qwKilledTasks = 0;
static QWORD g_qwDemandPages = 0;
static QWORD g_qwCowCopies = 0;
static QWORD g_qwCowReuses = 0;


// VMA는 있는데 프레임이 없다. 처음 건드릴 때 붙인다
static BOOL kAnonymousFault(mm_t* poMm, vm_area_t* poVma, QWORD qwCR2)
{
	QWORD qwVirtAddr = PAGE_ALIGN_DOWN(qwCR2);
	QWORD qwPhys;

	qwPhys = kAllocPage();
	if(0 == qwPhys) {
		return FALSE;
	}

	// 0으로 주지 않으면 이전 소유자의 내용이 샌다
	kMemSet(__va(qwPhys), 0, PAGE_SIZE);

	if(FALSE == kMmMapPage(poMm, qwVirtAddr, qwPhys, poVma->qwFlags)) {
		kFreePage(qwPhys);
		return FALSE;
	}

	kInvlpg(qwVirtAddr);
	++g_qwDemandPages;
	return TRUE;
}


// 참조가 하나뿐이면 복사하지 않고 쓰기 권한만 돌려준다
static BOOL kCowFault(mm_t* poMm, vm_area_t* poVma, QWORD qwCR2)
{
	QWORD qwVirtAddr = PAGE_ALIGN_DOWN(qwCR2);
	QWORD qwOldPhys, qwNewPhys;

	qwOldPhys = kVirtToPhys(poMm->qwPML4, qwVirtAddr);
	if(0 == qwOldPhys) {
		return FALSE;
	}

	if(1 >= kGetPageRefCount(qwOldPhys)) {
		if(FALSE == kMmMapPage(poMm, qwVirtAddr, qwOldPhys, poVma->qwFlags)) {
			return FALSE;
		}
		kInvlpg(qwVirtAddr);
		++g_qwCowReuses;
		return TRUE;
	}

	qwNewPhys = kAllocPage();
	if(0 == qwNewPhys) {
		return FALSE;
	}
	kMemCpy(__va(qwNewPhys), __va(qwOldPhys), PAGE_SIZE);

	if(FALSE == kMmMapPage(poMm, qwVirtAddr, qwNewPhys, poVma->qwFlags)) {
		kFreePage(qwNewPhys);
		return FALSE;
	}
	kInvlpg(qwVirtAddr);

	kPagePut(qwOldPhys);
	++g_qwCowCopies;
	return TRUE;
}


// 태스크를 여기서 바꾸지 않고 iretq가 돌아갈 자리만 kExitTask로 바꾼다
static void kKillFaultingTask(QWORD* pqwFrame, TCB_t* poTask)
{
	pqwFrame[PF_RIP_OFFSET]		= (QWORD)kExitTask;
	pqwFrame[PF_CS_OFFSET]		= GDT_KERNEL_CODE_SEGMENT;
	pqwFrame[PF_SS_OFFSET]		= GDT_KENNEL_DATA_SEGMENT;
	pqwFrame[PF_RSP_OFFSET]		= (QWORD)poTask->pvStackAddr + poTask->qwStackSize;
	pqwFrame[PF_RFLAGS_OFFSET]	&= ~RFLAGS_IF;

	pqwFrame[TASK_DS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	pqwFrame[TASK_ES_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	pqwFrame[TASK_FS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;
	pqwFrame[TASK_GS_OFFSET] = GDT_KENNEL_DATA_SEGMENT;

	++g_qwKilledTasks;
}


static void kReportSegv(TCB_t* poTask, QWORD qwCR2, QWORD qwErrCode, const char* pcWhy)
{
	char vcAddr[17];

	kToHexString(qwCR2, vcAddr, 16);
	kPrintf("SEGV task %q at %s (%s%s%s) %s\n",
			poTask->stLink.qwID, vcAddr,
			(qwErrCode & PF_ERR_WRITE) ? "write" : "read",
			(qwErrCode & PF_ERR_FETCH) ? ",exec" : "",
			(qwErrCode & PF_ERR_PRESENT) ? ",prot" : ",not-present",
			pcWhy);
}


BOOL kDoPageFault(QWORD qwErrCode, QWORD qwCR2, QWORD* pqwFrame)
{
	TCB_t* poTask;
	vm_area_t* poVma;

	// 커널 모드 폴트는 커널 버그다. 기존 패닉 경로로 넘긴다
	if(0 == (qwErrCode & PF_ERR_USER)) {
		return FALSE;
	}

	poTask = kGetRunningTask();

	// 커널 스레드는 poMM이 NULL. 안 걸러내면 핸들러 안에서 또 폴트난다
	if((NULL == poTask) || (NULL == poTask->poMM) || (NULL == poTask->pvStackAddr)) {
		return FALSE;
	}

	if(USER_VA_END <= qwCR2) {
		kReportSegv(poTask, qwCR2, qwErrCode, "kernel address");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	poVma = kVmaFind(poTask->poMM, qwCR2);
	if(NULL == poVma) {
		kReportSegv(poTask, qwCR2, qwErrCode, "no VMA");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	if((0 != (qwErrCode & PF_ERR_WRITE)) && (0 == (poVma->qwFlags & VM_WRITE))) {
		kReportSegv(poTask, qwCR2, qwErrCode, "VMA is read-only");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	if((0 != (qwErrCode & PF_ERR_FETCH)) && (0 == (poVma->qwFlags & VM_EXEC))) {
		kReportSegv(poTask, qwCR2, qwErrCode, "VMA is not executable");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	if(0 == (qwErrCode & PF_ERR_PRESENT)) {
		if(TRUE == kAnonymousFault(poTask->poMM, poVma, qwCR2)) {
			return TRUE;
		}
		kReportSegv(poTask, qwCR2, qwErrCode, "out of memory");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	// VMA는 쓰기를 허용하는데 PTE만 RO = COW
	if(0 != (qwErrCode & PF_ERR_WRITE)) {
		if(TRUE == kCowFault(poTask->poMM, poVma, qwCR2)) {
			return TRUE;
		}
		kReportSegv(poTask, qwCR2, qwErrCode, "out of memory (cow)");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	kReportSegv(poTask, qwCR2, qwErrCode, "protection");
	kKillFaultingTask(pqwFrame, poTask);
	return TRUE;
}


QWORD kGetCowCopyCount(void)
{
	return g_qwCowCopies;
}


QWORD kGetCowReuseCount(void)
{
	return g_qwCowReuses;
}


QWORD kGetDemandPageCount(void)
{
	return g_qwDemandPages;
}


QWORD kGetKilledTaskCount(void)
{
	return g_qwKilledTasks;
}
