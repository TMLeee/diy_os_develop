/*
 * bootmem.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  일회성 bump 할당자. mem_map과 비트맵을 놓을 자리를 확보하는 데만 쓰고
 *  물리 할당자가 인계받으면 동결한다.
 *
 *  mem_map을 .bss 정적 배열로 두면 안 되는 이유:
 *  커널 .bss는 01.Kernel32/src/main.c의 kInitKernel64Area()가 1MB~6MB를
 *  0으로 채워 주는 것에 전적으로 의존한다(objcopy가 NOBITS를 이미지에 넣지
 *  않으므로). RAM이 커지면 mem_map이 그 6MB 천장을 넘어 부트 스택까지
 *  덮어쓰는데, 아무 에러도 나지 않는다.
 */

#include "bootmem.h"
#include "mm.h"
#include "memmap.h"
#include "paging.h"
#include "descriptor.h"
#include "utility.h"


static QWORD g_qwBootmemStart = 0;
static QWORD g_qwBootmemNext = 0;
static QWORD g_qwBootmemLimit = 0;
static BOOL g_bBootmemFrozen = TRUE;


// 하드코딩된 정적 레이아웃의 최상단. 상수를 새로 만들지 않고 실제 정의에서
// 유도한다. 스텝 20/22에서 풀이 할당자로 옮겨가면 이 값도 같이 내려간다
static QWORD kGetStaticLayoutTop(void)
{
	// 심볼은 가상주소다. 여기서 필요한 건 물리 상한이다
	QWORD qwTop = __pa(__kernel_end);
	QWORD qwCandidate;

	qwCandidate = IST_START_ADDR + IST_SIZE;
	if(qwCandidate > qwTop) {
		qwTop = qwCandidate;
	}

	return PAGE_ALIGN_UP(qwTop);
}


BOOL kInitializeBootmem(void)
{
	QWORD qwStart = kGetStaticLayoutTop();
	const E820Entry_t* poEntry;
	int i;

	g_bBootmemFrozen = TRUE;
	g_qwBootmemStart = 0;
	g_qwBootmemNext = 0;
	g_qwBootmemLimit = 0;

	// qwStart를 품고 있는 USABLE 구간을 찾아 그 끝까지를 한계로 삼는다
	for(i=0; i<kGetE820Count(); ++i) {
		poEntry = kGetE820Entry(i);
		if(E820_TYPE_USABLE != poEntry->dwType) {
			continue;
		}
		if((poEntry->qwBase <= qwStart) &&
		   (qwStart < (poEntry->qwBase + poEntry->qwLength))) {
			g_qwBootmemStart = qwStart;
			g_qwBootmemNext = qwStart;
			g_qwBootmemLimit = PAGE_ALIGN_DOWN(poEntry->qwBase + poEntry->qwLength);
			g_bBootmemFrozen = FALSE;
			return TRUE;
		}
	}

	return FALSE;
}


void* kBootmemAlloc(QWORD qwSize)
{
	QWORD qwAddr;

	if((TRUE == g_bBootmemFrozen) || (0 == qwSize)) {
		return NULL;
	}

	qwSize = PAGE_ALIGN_UP(qwSize);
	if((g_qwBootmemNext + qwSize) > g_qwBootmemLimit) {
		return NULL;
	}

	qwAddr = g_qwBootmemNext;
	g_qwBootmemNext += qwSize;

	// 물리 프레임을 direct map을 통해 만진다. 반환값도 가상주소다 -
	kMemSet(__va(qwAddr), 0, (int)qwSize);
	return __va(qwAddr);
}


QWORD kBootmemGetStart(void)
{
	return g_qwBootmemStart;
}


QWORD kBootmemGetUsed(void)
{
	return g_qwBootmemNext - g_qwBootmemStart;
}


QWORD kBootmemGetLimit(void)
{
	return g_qwBootmemLimit;
}


void kBootmemFreeze(void)
{
	g_bBootmemFrozen = TRUE;
}
