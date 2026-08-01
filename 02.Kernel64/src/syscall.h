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
#define SYS_GETPID		39
#define SYS_EXIT		60
#define SYS_UPTIME		201		// 커널 자체 확장(틱 수)

// 유저 버퍼를 한 번에 커널로 들이는 최대 길이
#define SYS_WRITE_MAX	256

// 실패는 -errno. 리눅스와 같은 규약이라 유저측 래퍼가 그대로 통한다
#define SYS_EBADF		9
#define SYS_EFAULT		14
#define SYS_ENOSYS		38


// 저장된 레지스터 프레임(Context_t 레이아웃)을 받아 RAX 자리에 결과를 쓴다
void kSyscallHandler(QWORD* pqwRegs);
QWORD kGetSyscallCount(void);

#endif
