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
#include "paging.h"
#include "descriptor.h"
#include "console.h"
#include "utility.h"


static page_t* g_poMemMap = NULL;
static QWORD* g_pqwFreeBitmap = NULL;		// 비트 1 = 할당 가능
static QWORD g_qwTotalPages = 0;
static QWORD g_qwFreePages = 0;
static QWORD g_qwReservedPages = 0;

static void kBuildBuddyLists(void);


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
			__pa(__kernel_end) - KERNEL_PHYS_BASE, "kernel image");
	kReserveRange(0x600000, 0x100000, "kernel boot stack");
	kReserveRange(IST_START_ADDR, IST_SIZE, "IST1 stack");
	kReserveRange(kBootmemGetStart(), kBootmemGetUsed(), "bootmem (mem_map/bitmap)");

	// 여기서부터는 물리 할당자만 메모리를 나눠 준다
	kBootmemFreeze();

	// 비트맵 상태를 order별 free list로 옮긴다
	kBuildBuddyLists();
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

	// 풀린 프레임을 buddy 리스트에 다시 태운다
	kBuildBuddyLists();
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


// ---- buddy allocator ----------------------------------------------------
// 비트맵 선형 스캔을 order별 free list로 교체한다. 외부 API는 그대로다.
// 짝(buddy)의 PFN은 pfn ^ (1 << order)로 구한다

static KListHead_t g_vstFreeArea[PMM_MAX_ORDER];
static QWORD g_vqFreeCount[PMM_MAX_ORDER];


static void kBuddyPush(QWORD qwPfn, int iOrder)
{
	page_t* poPage = &(g_poMemMap[qwPfn]);

	poPage->iOrder = iOrder;
	poPage->qwFlags |= PG_BUDDY;
	kListAdd(&(poPage->stLru), &(g_vstFreeArea[iOrder]));
	++g_vqFreeCount[iOrder];
}


static void kBuddyRemove(QWORD qwPfn, int iOrder)
{
	page_t* poPage = &(g_poMemMap[qwPfn]);

	kListDel(&(poPage->stLru));
	poPage->qwFlags &= ~PG_BUDDY;
	--g_vqFreeCount[iOrder];
}


// 비트맵으로 표시된 free 프레임들을 order별 free list로 옮긴다.
// 정렬과 짝 조건을 만족하는 가장 큰 블록부터 묶는다
static void kBuildBuddyLists(void)
{
	QWORD qwPfn, qwCount, i;
	int iOrder;

	for(iOrder=0; iOrder<PMM_MAX_ORDER; ++iOrder) {
		kListInit(&(g_vstFreeArea[iOrder]));
		g_vqFreeCount[iOrder] = 0;
	}

	qwPfn = 0;
	while(qwPfn < g_qwTotalPages) {
		if(FALSE == kIsFrameFree(qwPfn)) {
			++qwPfn;
			continue;
		}

		// 이 위치에서 만들 수 있는 최대 블록을 찾는다
		for(iOrder=PMM_MAX_ORDER-1; iOrder>0; --iOrder) {
			qwCount = 1UL << iOrder;
			if(0 != (qwPfn & (qwCount - 1))) {
				continue;					// 정렬 안 됨
			}
			if((qwPfn + qwCount) > g_qwTotalPages) {
				continue;
			}
			for(i=0; i<qwCount; ++i) {
				if(FALSE == kIsFrameFree(qwPfn + i)) {
					break;
				}
			}
			if(i == qwCount) {
				break;						// 전부 free
			}
		}

		qwCount = 1UL << iOrder;
		kBuddyPush(qwPfn, iOrder);
		qwPfn += qwCount;
	}
}


QWORD kAllocPages(int iOrder)
{
	int iCur;
	QWORD qwPfn, qwBuddyPfn, i;
	KListHead_t* poEntry;

	if((NULL == g_poMemMap) || (iOrder < 0) || (PMM_MAX_ORDER <= iOrder)) {
		return 0;
	}

	// 요청 이상의 가장 작은 order에서 꺼낸다
	for(iCur=iOrder; iCur<PMM_MAX_ORDER; ++iCur) {
		if(FALSE == kListIsEmpty(&(g_vstFreeArea[iCur]))) {
			break;
		}
	}
	if(PMM_MAX_ORDER == iCur) {
		return 0;
	}

	poEntry = g_vstFreeArea[iCur].poNext;
	qwPfn = (QWORD)(KCONTAINER_OF(poEntry, page_t, stLru) - g_poMemMap);
	kBuddyRemove(qwPfn, iCur);

	// 요청 크기까지 반으로 쪼개면서 위쪽 반을 되돌린다
	while(iCur > iOrder) {
		--iCur;
		qwBuddyPfn = qwPfn + (1UL << iCur);
		kBuddyPush(qwBuddyPfn, iCur);
	}

	for(i=0; i<(1UL << iOrder); ++i) {
		kClearFrameFree(qwPfn + i);
		g_poMemMap[qwPfn + i].iRefCount = 1;
	}
	g_poMemMap[qwPfn].iOrder = iOrder;
	g_qwFreePages -= (1UL << iOrder);
	return PFN_PHYS(qwPfn);
}


// COW로 프레임을 공유하기 시작하면 마지막 소유자만 반납해야 한다.
// kAllocPages가 이미 iRefCount=1로 만들어 주므로 여기서는 증감만 한다
int kGetPageRefCount(QWORD qwPhysAddr)
{
	QWORD qwPfn = PFN_DOWN(qwPhysAddr);

	if((NULL == g_poMemMap) || (qwPfn >= g_qwTotalPages)) {
		return 0;
	}
	return g_poMemMap[qwPfn].iRefCount;
}


void kPageGet(QWORD qwPhysAddr)
{
	QWORD qwPfn = PFN_DOWN(qwPhysAddr);

	if((NULL == g_poMemMap) || (qwPfn >= g_qwTotalPages)) {
		return;
	}
	++g_poMemMap[qwPfn].iRefCount;
}


// 참조를 하나 내려놓는다. 0이 되면 그때 진짜로 반납한다
void kPagePut(QWORD qwPhysAddr)
{
	QWORD qwPfn = PFN_DOWN(qwPhysAddr);

	if((NULL == g_poMemMap) || (qwPfn >= g_qwTotalPages)) {
		return;
	}
	if(0 < g_poMemMap[qwPfn].iRefCount) {
		--g_poMemMap[qwPfn].iRefCount;
	}
	if(0 == g_poMemMap[qwPfn].iRefCount) {
		kFreePages(qwPhysAddr, 0);
	}
}


void kFreePages(QWORD qwPhysAddr, int iOrder)
{
	QWORD qwPfn = PFN_DOWN(qwPhysAddr);
	QWORD qwBuddyPfn, i;

	if((NULL == g_poMemMap) || (iOrder < 0) || (PMM_MAX_ORDER <= iOrder)) {
		return;
	}
	if((qwPfn + (1UL << iOrder)) > g_qwTotalPages) {
		return;
	}

	for(i=0; i<(1UL << iOrder); ++i) {
		if(g_poMemMap[qwPfn + i].qwFlags & PG_RESERVED) {
			return;
		}
		if(TRUE == kIsFrameFree(qwPfn + i)) {
			return;
		}
	}

	for(i=0; i<(1UL << iOrder); ++i) {
		g_poMemMap[qwPfn + i].qwFlags &= ~(PG_SLAB | PG_BUDDY);
		g_poMemMap[qwPfn + i].iRefCount = 0;
		g_poMemMap[qwPfn + i].pvPrivate = NULL;
		kSetFrameFree(qwPfn + i);
	}
	g_qwFreePages += (1UL << iOrder);

	// 짝이 같은 order로 비어 있으면 합친다
	while(iOrder < (PMM_MAX_ORDER - 1)) {
		qwBuddyPfn = qwPfn ^ (1UL << iOrder);

		if((qwBuddyPfn + (1UL << iOrder)) > g_qwTotalPages) {
			break;
		}
		// PG_RESERVED 프레임을 넘어 병합하면 0xA0000 구멍을 가로지른다
		if(g_poMemMap[qwBuddyPfn].qwFlags & PG_RESERVED) {
			break;
		}
		if(0 == (g_poMemMap[qwBuddyPfn].qwFlags & PG_BUDDY)) {
			break;
		}
		if(g_poMemMap[qwBuddyPfn].iOrder != iOrder) {
			break;
		}

		kBuddyRemove(qwBuddyPfn, iOrder);
		if(qwBuddyPfn < qwPfn) {
			qwPfn = qwBuddyPfn;
		}
		++iOrder;
	}

	kBuddyPush(qwPfn, iOrder);
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
	int i;

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

	// g_poMemMap은 이제 direct map 주소다. 물리로 환산해 보여 준다
	kToHexString(__pa(g_poMemMap), vcHex, 12);
	kUIToDecString(g_qwTotalPages * sizeof(page_t) / 1024, vcTotal);
	kPrintf("mem_map at %s (%sKB)\n", vcHex, vcTotal);

	kToHexString(kBootmemGetStart(), vcHex, 12);
	kUIToDecString(kBootmemGetUsed() / 1024, vcTotal);
	kPrintf("bootmem  at %s used %sKB\n", vcHex, vcTotal);

	// /proc/buddyinfo 대응. 병합이 제대로 되는지는 이 분포로 확인한다
	kPrintf("buddy:");
	for(i=0; i<PMM_MAX_ORDER; ++i) {
		kUIToDecString(g_vqFreeCount[i], vcTotal);
		kPrintf(" %s", vcTotal);
	}
	kPrintf("\n");
}
