/*
 * pmm.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  물리 프레임 할당자. 지금은 비트맵 스캔이지만 API는 buddy와 동일하다.
 *  mem_map(page_t 배열)은 처음부터 만들어 둔다 - slab의 O(1) kfree와
 *  COW의 refcount가 여기에 의존한다.
 */

#include "pmm.h"
#include "memmap.h"
#include "bootmem.h"
#include "descriptor.h"
#include "task.h"
#include "console.h"
#include "utility.h"


static page_t* g_poMemMap = NULL;
static QWORD* g_pqwFreeBitmap = NULL;		// 비트 1 = 할당 가능
static QWORD g_qwTotalPages = 0;
static QWORD g_qwFreePages = 0;
static QWORD g_qwReservedPages = 0;


static inline BOOL kIsFrameFree(QWORD qwPfn)
{
	return (g_pqwFreeBitmap[qwPfn >> 6] & (1UL << (qwPfn & 63))) ? TRUE : FALSE;
}


static inline void kSetFrameFree(QWORD qwPfn)
{
	g_pqwFreeBitmap[qwPfn >> 6] |= (1UL << (qwPfn & 63));
}


static inline void kClearFrameFree(QWORD qwPfn)
{
	g_pqwFreeBitmap[qwPfn >> 6] &= ~(1UL << (qwPfn & 63));
}


// [qwBase, qwBase+qwSize) 범위를 영구 예약한다
static void kReserveRange(QWORD qwBase, QWORD qwSize, const char* pcWhat)
{
	QWORD qwPfn = PFN_DOWN(PAGE_ALIGN_DOWN(qwBase));
	QWORD qwEndPfn = PFN_UP(qwBase + qwSize);

	(void)pcWhat;

	if(qwEndPfn > g_qwTotalPages) {
		qwEndPfn = g_qwTotalPages;
	}

	for(; qwPfn < qwEndPfn; ++qwPfn) {
		if(TRUE == kIsFrameFree(qwPfn)) {
			kClearFrameFree(qwPfn);
			--g_qwFreePages;
		}
		if(0 == (g_poMemMap[qwPfn].qwFlags & PG_RESERVED)) {
			g_poMemMap[qwPfn].qwFlags |= PG_RESERVED;
			++g_qwReservedPages;
		}
	}
}


BOOL kInitializePhysicalMemory(void)
{
	QWORD qwHighest = kGetHighestUsableAddr();
	QWORD qwMemMapSize, qwBitmapSize, qwPfn, qwEndPfn;
	const E820Entry_t* poEntry;
	int i;

	if(0 == qwHighest) {
		return FALSE;
	}

	if(FALSE == kInitializeBootmem()) {
		return FALSE;
	}

	g_qwTotalPages = PFN_UP(qwHighest);
	g_qwFreePages = 0;
	g_qwReservedPages = 0;

	// mem_map과 비트맵을 bootmem에서 확보(.bss에 두면 6MB 천장을 넘는다)
	qwMemMapSize = g_qwTotalPages * sizeof(page_t);
	qwBitmapSize = ((g_qwTotalPages + 63) / 64) * sizeof(QWORD);

	g_poMemMap = (page_t*)kBootmemAlloc(qwMemMapSize);
	if(NULL == g_poMemMap) {
		return FALSE;
	}
	g_pqwFreeBitmap = (QWORD*)kBootmemAlloc(qwBitmapSize);
	if(NULL == g_pqwFreeBitmap) {
		return FALSE;
	}

	// bootmem이 0으로 채워 주므로 전부 '예약 아님 / 할당 불가' 상태로 시작
	for(qwPfn=0; qwPfn<g_qwTotalPages; ++qwPfn) {
		g_poMemMap[qwPfn].iOrder = 0;
		g_poMemMap[qwPfn].iRefCount = 0;
		g_poMemMap[qwPfn].pvPrivate = NULL;
		kListInit(&(g_poMemMap[qwPfn].stLru));
	}

	// USABLE 구간만 할당 가능으로 표시
	for(i=0; i<kGetE820Count(); ++i) {
		poEntry = kGetE820Entry(i);
		if(E820_TYPE_USABLE != poEntry->dwType) {
			continue;
		}
		qwPfn = PFN_UP(poEntry->qwBase);
		qwEndPfn = PFN_DOWN(poEntry->qwBase + poEntry->qwLength);
		if(qwEndPfn > g_qwTotalPages) {
			qwEndPfn = g_qwTotalPages;
		}
		for(; qwPfn < qwEndPfn; ++qwPfn) {
			if(FALSE == kIsFrameFree(qwPfn)) {
				kSetFrameFree(qwPfn);
				++g_qwFreePages;
			}
		}
	}

	// USABLE 안에 들어 있는 커널 구조물을 되돌려 예약한다
	kReserveRange(0, 0x100000, "BIOS/IVT/VGA/bootinfo");
	kReserveRange(KERNEL32_PAGETABLE_BASE, KERNEL32_PAGETABLE_SIZE, "Kernel32 page tables");
	kReserveRange(GDTR_START_ADDR, PAGE_SIZE, "GDT/TSS/IDT");
	kReserveRange(KERNEL_PHYS_BASE,
			(QWORD)__kernel_end - KERNEL_PHYS_BASE, "kernel image");
	kReserveRange(0x600000, 0x100000, "kernel boot stack");
	kReserveRange(IST_START_ADDR, IST_SIZE, "IST1 stack");
	kReserveRange(TASK_TCB_POLL_ADDR,
			(TASK_STACK_POOL_ADDR + ((QWORD)TASK_STACK_SIZE * TASK_MAX_CNT))
			- TASK_TCB_POLL_ADDR, "TCB + task stack pools");
	kReserveRange(kBootmemGetStart(), kBootmemGetUsed(), "bootmem (mem_map/bitmap)");

	// 여기서부터는 물리 할당자만 메모리를 나눠 준다
	kBootmemFreeze();
	return TRUE;
}


// 예약을 풀고 할당 가능으로 되돌린다. 그 영역이 정말 죽었는지는 호출자 책임
void kUnreserveRange(QWORD qwBase, QWORD qwSize)
{
	QWORD qwPfn = PFN_UP(qwBase);
	QWORD qwEndPfn = PFN_DOWN(qwBase + qwSize);

	if(qwEndPfn > g_qwTotalPages) {
		qwEndPfn = g_qwTotalPages;
	}

	for(; qwPfn < qwEndPfn; ++qwPfn) {
		if(0 == (g_poMemMap[qwPfn].qwFlags & PG_RESERVED)) {
			continue;
		}
		g_poMemMap[qwPfn].qwFlags &= ~PG_RESERVED;
		--g_qwReservedPages;

		if(FALSE == kIsFrameFree(qwPfn)) {
			kSetFrameFree(qwPfn);
			++g_qwFreePages;
		}
	}
}


page_t* kPfnToPage(QWORD qwPfn)
{
	if((NULL == g_poMemMap) || (qwPfn >= g_qwTotalPages)) {
		return NULL;
	}
	return &(g_poMemMap[qwPfn]);
}


page_t* kPhysToPage(QWORD qwPhysAddr)
{
	return kPfnToPage(PFN_DOWN(qwPhysAddr));
}


QWORD kPageToPhys(const page_t* poPage)
{
	if((NULL == g_poMemMap) || (NULL == poPage)) {
		return 0;
	}
	return PFN_PHYS((QWORD)(poPage - g_poMemMap));
}


// 2^iOrder 프레임을 그 크기에 정렬해서 찾는다. buddy로 교체할 때
// 호출부가 바뀌지 않도록 정렬 조건을 지금부터 지킨다
QWORD kAllocPages(int iOrder)
{
	QWORD qwCount, qwPfn, qwStep, i;
	BOOL bAllFree;

	if((NULL == g_pqwFreeBitmap) || (iOrder < 0) || (PMM_MAX_ORDER <= iOrder)) {
		return 0;
	}

	qwCount = 1UL << iOrder;
	qwStep = qwCount;

	for(qwPfn=0; (qwPfn + qwCount) <= g_qwTotalPages; qwPfn += qwStep) {
		bAllFree = TRUE;
		for(i=0; i<qwCount; ++i) {
			if(FALSE == kIsFrameFree(qwPfn + i)) {
				bAllFree = FALSE;
				break;
			}
		}
		if(FALSE == bAllFree) {
			continue;
		}

		for(i=0; i<qwCount; ++i) {
			kClearFrameFree(qwPfn + i);
			g_poMemMap[qwPfn + i].iRefCount = 1;
		}
		g_poMemMap[qwPfn].iOrder = iOrder;
		g_qwFreePages -= qwCount;
		return PFN_PHYS(qwPfn);
	}

	return 0;
}


void kFreePages(QWORD qwPhysAddr, int iOrder)
{
	QWORD qwPfn = PFN_DOWN(qwPhysAddr);
	QWORD qwCount = 1UL << iOrder;
	QWORD i;

	if((NULL == g_pqwFreeBitmap) || (iOrder < 0) || (PMM_MAX_ORDER <= iOrder)) {
		return;
	}
	if((qwPfn + qwCount) > g_qwTotalPages) {
		return;
	}

	for(i=0; i<qwCount; ++i) {
		// 예약된 프레임이나 이미 free인 프레임은 건드리지 않는다
		if(g_poMemMap[qwPfn + i].qwFlags & PG_RESERVED) {
			return;
		}
		if(TRUE == kIsFrameFree(qwPfn + i)) {
			return;
		}
	}

	for(i=0; i<qwCount; ++i) {
		g_poMemMap[qwPfn + i].iRefCount = 0;
		g_poMemMap[qwPfn + i].pvPrivate = NULL;
		kSetFrameFree(qwPfn + i);
	}
	g_poMemMap[qwPfn].iOrder = 0;
	g_qwFreePages += qwCount;
}


QWORD kGetTotalPageCount(void)
{
	return g_qwTotalPages;
}


QWORD kGetFreePageCount(void)
{
	return g_qwFreePages;
}


QWORD kGetReservedPageCount(void)
{
	return g_qwReservedPages;
}


void kPrintPhysicalMemoryStat(void)
{
	char vcTotal[24], vcFree[24], vcRes[24], vcHex[17];

	if(NULL == g_poMemMap) {
		kPrintf("Physical memory manager not initialized\n");
		return;
	}

	kUIToDecString(g_qwTotalPages, vcTotal);
	kUIToDecString(g_qwFreePages, vcFree);
	kUIToDecString(g_qwReservedPages, vcRes);
	kPrintf("frames total=%s free=%s reserved=%s\n", vcTotal, vcFree, vcRes);

	kUIToDecString((g_qwFreePages * PAGE_SIZE) / 0x100000, vcFree);
	kUIToDecString((g_qwTotalPages * PAGE_SIZE) / 0x100000, vcTotal);
	kPrintf("memory free=%sMB of %sMB\n", vcFree, vcTotal);

	kToHexString((QWORD)g_poMemMap, vcHex, 12);
	kUIToDecString(g_qwTotalPages * sizeof(page_t) / 1024, vcTotal);
	kPrintf("mem_map at %s (%sKB)\n", vcHex, vcTotal);

	kToHexString(kBootmemGetStart(), vcHex, 12);
	kUIToDecString(kBootmemGetUsed() / 1024, vcTotal);
	kPrintf("bootmem  at %s used %sKB\n", vcHex, vcTotal);
}
