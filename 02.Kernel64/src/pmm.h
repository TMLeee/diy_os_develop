/*
 * pmm.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_PMM_H_
#define __02_KERNEL64_SRC_PMM_H_

#include "types.h"
#include "mm.h"

// buddy의 최대 order (2^10 * 4KB = 4MB). 지금은 비트맵 구현이지만
// API를 처음부터 buddy 모양으로 노출해서 교체 시 호출부가 바뀌지 않게 한다
#define PMM_MAX_ORDER			11


BOOL kInitializePhysicalMemory(void);

QWORD kAllocPages(int iOrder);
void kFreePages(QWORD qwPhysAddr, int iOrder);
#define kAllocPage()			kAllocPages(0)
#define kFreePage(pa)			kFreePages((pa), 0)

page_t* kPhysToPage(QWORD qwPhysAddr);
QWORD kPageToPhys(const page_t* poPage);
page_t* kPfnToPage(QWORD qwPfn);

void kUnreserveRange(QWORD qwBase, QWORD qwSize);
QWORD kGetTotalPageCount(void);
QWORD kGetFreePageCount(void);
QWORD kGetReservedPageCount(void);
void kPrintPhysicalMemoryStat(void);


#endif /* 02_KERNEL64_SRC_PMM_H_ */
