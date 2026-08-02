/*
 * vmalloc.h
 *
 *  Created on: 2026. 7. 31.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_VMALLOC_H_
#define __02_KERNEL64_SRC_VMALLOC_H_

#include "types.h"

// direct map(PML4[256]) 위쪽의 별도 창. 물리적으로 흩어진 프레임을
// 연속된 가상주소로 묶는 데 쓴다
#define VMALLOC_START		0xFFFFC90000000000UL
#define VMALLOC_END			0xFFFFE90000000000UL


BOOL kInitializeVmalloc(void);

// iGuardBefore/After 만큼 앞뒤에 매핑하지 않은 페이지를 남긴다.
// 스택 오버플로를 조용한 손상 대신 #PF로 만드는 수단
void* kVmapPages(int iPageCount, int iGuardBefore, int iGuardAfter);
void* kVmalloc(QWORD qwSize);
void kVfree(void* pvAddr);

BOOL kIsVmallocAddr(QWORD qwVirtAddr);
BOOL kIsVmallocGuardPage(QWORD qwVirtAddr);
void kPrintVmallocInfo(void);


#endif /* 02_KERNEL64_SRC_VMALLOC_H_ */
