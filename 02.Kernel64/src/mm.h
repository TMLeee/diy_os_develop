/*
 * mm.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  메모리 관리 공통 정의
 */

#ifndef __02_KERNEL64_SRC_MM_H_
#define __02_KERNEL64_SRC_MM_H_

#include "types.h"
#include "klist.h"

#define PAGE_SHIFT				12
#define PAGE_SIZE				(1UL << PAGE_SHIFT)			// 4096
#define PAGE_MASK				(~(PAGE_SIZE - 1))

#define ALIGN_DOWN(x, a)		((QWORD)(x) & ~((QWORD)(a) - 1))
#define ALIGN_UP(x, a)			ALIGN_DOWN((QWORD)(x) + (QWORD)(a) - 1, (a))

#define PAGE_ALIGN_DOWN(x)		ALIGN_DOWN(x, PAGE_SIZE)
#define PAGE_ALIGN_UP(x)		ALIGN_UP(x, PAGE_SIZE)

#define PFN_DOWN(x)				((QWORD)(x) >> PAGE_SHIFT)
#define PFN_UP(x)				(((QWORD)(x) + PAGE_SIZE - 1) >> PAGE_SHIFT)
#define PFN_PHYS(pfn)			((QWORD)(pfn) << PAGE_SHIFT)

// 커널 이미지의 링크 주소와 끝(elf_x86_64.x)
#define KERNEL_PHYS_BASE		0x200000
extern char __kernel_end[];

// page_t.qwFlags
#define PG_RESERVED				0x0001		// 할당자가 절대 내주지 않는 프레임
#define PG_BUDDY				0x0002		// buddy free_area에 들어있음
#define PG_SLAB					0x0004		// slab이 쓰는 중

// 물리 프레임 하나의 상태. 리눅스 struct page에 대응
typedef struct kPageStruct {
	QWORD		qwFlags;
	int			iOrder;
	int			iRefCount;		// COW에서 사용
	KListHead_t	stLru;			// free_area 또는 slab 리스트 링크
	void*		pvPrivate;		// 소유 kmem_cache
}page_t;

#endif /* 02_KERNEL64_SRC_MM_H_ */
