/*
 * syscall.h
 *
 *  Created on: 2026. 8. 1.
 *      Author: Macbook_pro
 *
 *  int 0x80 시스템 콜. 번호는 리눅스 x86-64 것을 그대로 쓴다
 */

#ifndef __SYSCALL_H__
#define __SYSCALL_H__

#include "types.h"


#define SYS_WRITE		1
#define SYS_FORK		57
#define SYS_GETPID		39
#define SYS_EXIT		60
#define SYS_UPTIME		201		// 커널 자체 확장(틱 수)

#define SYS_WRITE_MAX	256

// 실패는 -errno (리눅스 규약)
#define SYS_EBADF		9
#define SYS_EFAULT		14
#define SYS_ENOSYS		38
#define SYS_EINVAL		22
#define SYS_ENOMEM		12


void kSyscallHandler(QWORD* pqwRegs);
QWORD kGetSyscallCount(void);

#endif
