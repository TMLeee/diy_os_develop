/*
 * syscall.c
 *
 *  Created on: 2026. 8. 1.
 *      Author: Macbook_pro
 */


#include "syscall.h"
#include "task.h"
#include "console.h"
#include "utility.h"
#include "mm_struct.h"


static QWORD g_qwSyscallCount = 0;


// 유저 포인터를 아직 VMA로 검증하지 않는다. kCopyFromUser가 들어갈 자리
static QWORD kSysWrite(QWORD qwFd, const char* pcBuf, QWORD qwLen)
{
	char vcTmp[SYS_WRITE_MAX + 1];
	QWORD i;

	if((1 != qwFd) && (2 != qwFd)) {
		return (QWORD)(-SYS_EBADF);
	}
	if(NULL == pcBuf) {
		return (QWORD)(-SYS_EFAULT);
	}

	if(SYS_WRITE_MAX < qwLen) {
		qwLen = SYS_WRITE_MAX;
	}
	for(i=0; i<qwLen; ++i) {
		vcTmp[i] = pcBuf[i];
	}
	vcTmp[qwLen] = '\0';

	// kConsolePrintString은 새 출력 위치를 반환만 하고 커밋하지 않는다.
	// 커서를 옮기는 건 kPrintf 쪽이라 그걸 거쳐야 다음 출력이 덮어쓰지 않는다
	kPrintf("%s", vcTmp);
	return qwLen;
}


static QWORD kSysGetPid(void)
{
	TCB_t* poTask = kGetRunningTask();

	if(NULL == poTask) {
		return (QWORD)(-SYS_EFAULT);
	}
	return poTask->stLink.qwID;
}


// 주소공간을 복사하지 않는다. 공유해 두고 쓸 때 폴트에서 복사한다
static QWORD kSysFork(QWORD* pqwRegs)
{
	TCB_t* poParent = kGetRunningTask();
	TCB_t* poChild;
	mm_t* poChildMm;

	if((NULL == poParent) || (NULL == poParent->poMM)) {
		return (QWORD)(-SYS_EINVAL);
	}

	poChildMm = kMmCopy(poParent->poMM);
	if(NULL == poChildMm) {
		return (QWORD)(-SYS_ENOMEM);
	}

	poChild = kForkTask(poChildMm, pqwRegs);
	if(NULL == poChild) {
		kMmDestroy(poChildMm);
		return (QWORD)(-SYS_ENOMEM);
	}

	return poChild->stLink.qwID;
}


void kSyscallHandler(QWORD* pqwRegs)
{
	QWORD qwNum = pqwRegs[TASK_RAX_OFFSET];
	QWORD qwRet;

	++g_qwSyscallCount;

	switch(qwNum) {
		case SYS_WRITE:
			qwRet = kSysWrite(pqwRegs[TASK_RDI_OFFSET],
							  (const char*)pqwRegs[TASK_RSI_OFFSET],
							  pqwRegs[TASK_RDX_OFFSET]);
			break;

		case SYS_FORK:
			qwRet = kSysFork(pqwRegs);
			break;

		case SYS_GETPID:
			qwRet = kSysGetPid();
			break;

		case SYS_UPTIME:
			qwRet = g_qwTickCount;
			break;

		default:
			qwRet = (QWORD)(-SYS_ENOSYS);
			break;
	}

	// KLOADCONTEXT가 pop할 RAX 자리에 넣는다
	pqwRegs[TASK_RAX_OFFSET] = qwRet;
}


QWORD kGetSyscallCount(void)
{
	return g_qwSyscallCount;
}
