/*
 * fault.h
 *
 *  Created on: 2026. 8. 2.
 *      Author: Macbook_pro
 *
 *  #PF 처리. 리눅스 do_page_fault에 대응
 */

#ifndef __FAULT_H__
#define __FAULT_H__

#include "types.h"


// #PF 에러코드 비트
#define PF_ERR_PRESENT	0x01		// 0이면 not-present, 1이면 권한 위반
#define PF_ERR_WRITE	0x02
#define PF_ERR_USER		0x04
#define PF_ERR_RSVD		0x08
#define PF_ERR_FETCH	0x10		// 명령어 인출


// 처리했으면 TRUE. FALSE면 호출자가 패닉 경로로 넘긴다
BOOL kDoPageFault(QWORD qwErrCode, QWORD qwCR2, QWORD* pqwFrame);

QWORD kGetKilledTaskCount(void);
QWORD kGetDemandPageCount(void);
QWORD kGetCowCopyCount(void);
QWORD kGetCowReuseCount(void);

#endif
