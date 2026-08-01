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


// 에러코드가 있는 예외는 프레임이 한 칸 밀린다. KSAVECONTEXT가 rbp를 먼저
// 밀어 넣고, CPU가 남긴 에러코드가 원래 RIP가 있을 자리를 차지한다.
// ds/es/fs/gs는 프레임 바닥이라 밀리지 않는다
#define PF_RIP_OFFSET		(TASK_RIP_OFFSET + 1)
#define PF_CS_OFFSET		(TASK_CS_OFFSET + 1)
#define PF_RFLAGS_OFFSET	(TASK_RFLAGS_OFFSET + 1)
#define PF_RSP_OFFSET		(TASK_RSP_OFFSET + 1)
#define PF_SS_OFFSET		(TASK_SS_OFFSET + 1)

#define RFLAGS_IF			0x0200


static QWORD g_qwKilledTasks = 0;
static QWORD g_qwDemandPages = 0;


// VMA는 있는데 페이지가 없다 = 익명 페이지 지연 할당. 리눅스가 익명 매핑에
// 하는 것과 같다 - 주소공간을 잡을 때가 아니라 처음 건드릴 때 프레임을 준다
static BOOL kAnonymousFault(mm_t* poMm, vm_area_t* poVma, QWORD qwCR2)
{
	QWORD qwVirtAddr = PAGE_ALIGN_DOWN(qwCR2);
	QWORD qwPhys;

	qwPhys = kAllocPage();
	if(0 == qwPhys) {
		return FALSE;
	}

	// 반드시 0으로 준다. 안 그러면 앞서 이 프레임을 쓰던 쪽의 내용이 샌다
	kMemSet(__va(qwPhys), 0, PAGE_SIZE);

	if(FALSE == kMmMapPage(poMm, qwVirtAddr, qwPhys, poVma->qwFlags)) {
		kFreePage(qwPhys);
		return FALSE;
	}

	// 이 VA로 not-present가 TLB에 캐시돼 있을 수 있다
	kInvlpg(qwVirtAddr);
	++g_qwDemandPages;
	return TRUE;
}


// 폴트를 낸 유저 태스크를 끝낸다. 여기서 직접 태스크를 바꾸지 않고 iretq가
// 돌아갈 자리만 커널 모드의 kExitTask로 바꿔 둔다. 그러면 복귀 경로가
// 평소와 똑같이 유지되고, 정리는 자기 커널 스택 위에서 안전하게 돈다
static void kKillFaultingTask(QWORD* pqwFrame, TCB_t* poTask)
{
	pqwFrame[PF_RIP_OFFSET]		= (QWORD)kExitTask;
	pqwFrame[PF_CS_OFFSET]		= GDT_KERNEL_CODE_SEGMENT;
	pqwFrame[PF_SS_OFFSET]		= GDT_KENNEL_DATA_SEGMENT;
	pqwFrame[PF_RSP_OFFSET]		= (QWORD)poTask->pvStackAddr + poTask->qwStackSize;
	pqwFrame[PF_RFLAGS_OFFSET]	&= ~RFLAGS_IF;

	// KLOADCONTEXT가 적재하는 값들도 커널 것으로 되돌린다
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

	// 커널 모드 폴트는 전부 커널 버그다. 기존 덤프/패닉 경로로 넘긴다
	if(0 == (qwErrCode & PF_ERR_USER)) {
		return FALSE;
	}

	poTask = kGetRunningTask();

	// 커널 스레드는 poMM이 NULL이다. 먼저 걸러내지 않으면 폴트 핸들러 안에서
	// NULL을 역참조하며 두 번째 폴트가 난다
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

	// 여기까지 왔는데 not-present면 정당한 접근인데 프레임이 없는 것이다
	if(0 == (qwErrCode & PF_ERR_PRESENT)) {
		if(TRUE == kAnonymousFault(poTask->poMM, poVma, qwCR2)) {
			return TRUE;
		}
		kReportSegv(poTask, qwCR2, qwErrCode, "out of memory");
		kKillFaultingTask(pqwFrame, poTask);
		return TRUE;
	}

	// present인데 여기까지 온 것은 아직 다루지 않는 권한 위반이다.
	// COW가 들어오는 자리다(26c)
	kReportSegv(poTask, qwCR2, qwErrCode, "protection");
	kKillFaultingTask(pqwFrame, poTask);
	return TRUE;
}


QWORD kGetDemandPageCount(void)
{
	return g_qwDemandPages;
}


QWORD kGetKilledTaskCount(void)
{
	return g_qwKilledTasks;
}
