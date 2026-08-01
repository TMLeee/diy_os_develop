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

	// VMA는 있는데 페이지가 없는 경우 = demand paging. 26b에서 채운다
	kReportSegv(poTask, qwCR2, qwErrCode, "unpopulated VMA");
	kKillFaultingTask(pqwFrame, poTask);
	return TRUE;
}


QWORD kGetKilledTaskCount(void)
{
	return g_qwKilledTasks;
}
