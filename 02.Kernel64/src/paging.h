/*
 * paging.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  IA-32e 4레벨 페이징. 01.Kernel32/src/page.h는 4레벨을 모두 DWORD 2개짜리
 *  같은 구조체로 typedef해서 64비트에서 쓰기 어렵다. 여기서는 QWORD로 다룬다.
 */

#ifndef __02_KERNEL64_SRC_PAGING_H_
#define __02_KERNEL64_SRC_PAGING_H_

#include "types.h"
#include "mm.h"

// 4레벨 모두 같은 8바이트 포맷이다
typedef QWORD pte_t;

#define PTE_P			(1UL << 0)		// Present
#define PTE_RW			(1UL << 1)		// Writable
#define PTE_US			(1UL << 2)		// User accessible
#define PTE_PWT			(1UL << 3)
#define PTE_PCD			(1UL << 4)		// Cache disable
#define PTE_A			(1UL << 5)		// Accessed
#define PTE_D			(1UL << 6)		// Dirty
#define PTE_PS			(1UL << 7)		// Page Size (2MB/1GB)
#define PTE_G			(1UL << 8)		// Global
#define PTE_NX			(1UL << 63)		// No Execute

#define PTE_ADDR_MASK	0x000FFFFFFFFFF000UL
#define PTE_FLAG_MASK	(~PTE_ADDR_MASK)

#define PTE_ADDR(e)		((QWORD)(e) & PTE_ADDR_MASK)

#define PAGE_ENTRY_COUNT	512
#define PAGE_SIZE_2M		0x200000UL

// 가상주소에서 각 레벨의 인덱스를 뽑는다
#define PML4_INDEX(va)	(((QWORD)(va) >> 39) & 0x1FF)
#define PDPT_INDEX(va)	(((QWORD)(va) >> 30) & 0x1FF)
#define PD_INDEX(va)	(((QWORD)(va) >> 21) & 0x1FF)
#define PT_INDEX(va)	(((QWORD)(va) >> 12) & 0x1FF)

// 워크 결과 레벨
#define PG_LEVEL_NONE	0		// 매핑 없음
#define PG_LEVEL_1G		1		// PDPT에서 PS=1
#define PG_LEVEL_2M		2		// PD에서 PS=1
#define PG_LEVEL_4K		3		// PT 엔트리

// 리눅스식 direct map. 커널 이미지는 아직 0x200000에 그대로 두고
// 물리 메모리 전체를 여기에 한 번 더 매핑한다. -mcmodel=small은 심볼을
// 2GB 위에 못 두지만 계산된 64비트 포인터의 역참조는 문제없으므로
// 빌드 플래그를 바꾸지 않고도 동작한다. higher-half는 스텝 24
#define PAGE_OFFSET		0xFFFF800000000000UL	// PML4[256]
#define KERNEL_VMA		0xFFFFFFFF80000000UL	// 스텝 24 예약

#define __va(pa)		((void*)((QWORD)(pa) + PAGE_OFFSET))
QWORD __pa(const void* pvVirtAddr);

// IA32_EFER
#define MSR_IA32_EFER	0xC0000080
#define EFER_NXE		(1UL << 11)
#define CR0_WP			(1UL << 16)


// 지정한 CR3를 따라 va를 워크한다. 각 레벨의 엔트리 값을 vqEntry[0..3]에
// (PML4, PDPT, PD, PT 순) 채우고 도달한 레벨을 반환한다
int kWalkPageTable(QWORD qwCR3, QWORD qwVirtAddr, pte_t* pvqEntry);
QWORD kVirtToPhys(QWORD qwCR3, QWORD qwVirtAddr);
void kDumpPageWalk(QWORD qwCR3, QWORD qwVirtAddr);
const char* kGetPageLevelName(int iLevel);

BOOL kInitializePaging(void);
QWORD kGetKernelCR3(void);
BOOL kIsNXSupported(void);

// 4KB 단위 매핑. 중간 테이블이 없으면 프레임 할당자에서 만든다
BOOL kMapPage(QWORD qwCR3, QWORD qwVirtAddr, QWORD qwPhysAddr, QWORD qwFlags);
BOOL kMapRange(QWORD qwCR3, QWORD qwVirtAddr, QWORD qwPhysAddr,
		QWORD qwSize, QWORD qwFlags);
void kUnmapPage(QWORD qwCR3, QWORD qwVirtAddr);


#endif /* 02_KERNEL64_SRC_PAGING_H_ */
