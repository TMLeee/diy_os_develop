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


static QWORD g_qwSyscallCount = 0;


// 유저 버퍼는 아직 검증할 VMA가 없다. 길이만 자르고 커널 스택으로 복사해서
// 콘솔에 넘긴다. 스텝 25c에서 kCopyFromUser()로 승격시킬 자리다
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


void kSyscallHandler(QWORD* pqwRegs)
{
	QWORD qwNum = pqwRegs[TASK_RAX_OFFSET];
	QWORD qwRet;

	++g_qwSyscallCount;

	// 인자는 리눅스 x86-64 규약 그대로 RDI, RSI, RDX 순
	switch(qwNum) {
		case SYS_WRITE:
			qwRet = kSysWrite(pqwRegs[TASK_RDI_OFFSET],
							  (const char*)pqwRegs[TASK_RSI_OFFSET],
							  pqwRegs[TASK_RDX_OFFSET]);
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

	// 반환값은 저장된 RAX 자리에 넣는다. KLOADCONTEXT가 이걸 pop한다
	pqwRegs[TASK_RAX_OFFSET] = qwRet;
}


QWORD kGetSyscallCount(void)
{
	return g_qwSyscallCount;
}
