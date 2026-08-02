/*
 * slab.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  slab 할당자와 그 위의 kmalloc.
 *  kfree가 O(1)인 이유: 주소에서 프레임을 구하고 page_t.pvPrivate에 적힌
 *  소유 캐시를 바로 읽는다. mem_map을 미리 만들어 둔 대가를 여기서 회수한다.
 */

#include "slab.h"
#include "pmm.h"
#include "paging.h"
#include "console.h"
#include "utility.h"


// slab 하나의 머리. 프레임 맨 앞에 놓고 뒤쪽을 객체 배열로 쓴다
typedef struct kSlabStruct {
	KListHead_t		stLink;
	kmem_cache_t*	poCache;
	QWORD			qwFreeCount;
	QWORD			qwFirstFree;	// 프리 리스트 머리(객체 인덱스), 없으면 -1
	QWORD			qwPhysBase;
}slab_t;

#define SLAB_NO_FREE		((QWORD)-1)

// 캐시 서술자 자체를 담을 정적 배열(캐시의 캐시 문제를 피한다)
static kmem_cache_t gvstCacheTable[SLAB_MAX_CACHE];
static BOOL g_bSlabReady = FALSE;

// kmalloc용 2의 거듭제곱 캐시: 8,16,...,16384
#define KMALLOC_MIN_SHIFT	3
#define KMALLOC_MAX_SHIFT	14
#define KMALLOC_CACHE_CNT	(KMALLOC_MAX_SHIFT - KMALLOC_MIN_SHIFT + 1)
static kmem_cache_t* gvpoKmallocCache[KMALLOC_CACHE_CNT];


static QWORD kSlabObjOffset(const slab_t* poSlab, QWORD qwIndex)
{
	return ALIGN_UP(sizeof(slab_t), poSlab->poCache->qwAlign)
			+ (qwIndex * poSlab->poCache->qwObjSize);
}


// 새 slab을 만들어 캐시의 partial 리스트에 넣는다
static slab_t* kSlabGrow(kmem_cache_t* poCache)
{
	QWORD qwPhys, qwIndex;
	slab_t* poSlab;
	page_t* poPage;
	QWORD qwFrameCount, i;

	qwPhys = kAllocPages(poCache->iOrder);
	if(0 == qwPhys) {
		return NULL;
	}

	poSlab = (slab_t*)__va(qwPhys);
	kMemSet(poSlab, 0, sizeof(slab_t));
	kListInit(&(poSlab->stLink));
	poSlab->poCache = poCache;
	poSlab->qwPhysBase = qwPhys;		// 물리. 객체 주소 계산의 기준
	poSlab->qwFreeCount = poCache->iObjsPerSlab;
	poSlab->qwFirstFree = 0;

	// 프리 리스트를 객체 안에 심어 둔다(각 객체의 첫 QWORD가 다음 인덱스)
	for(qwIndex=0; qwIndex<(QWORD)poCache->iObjsPerSlab; ++qwIndex) {
		*(QWORD*)((QWORD)__va(qwPhys) + kSlabObjOffset(poSlab, qwIndex)) =
				(qwIndex + 1 < (QWORD)poCache->iObjsPerSlab) ? (qwIndex + 1) : SLAB_NO_FREE;
	}

	// 이 slab이 쓰는 모든 프레임에 소유 캐시를 기록한다. kfree가 이걸 본다
	qwFrameCount = 1UL << poCache->iOrder;
	for(i=0; i<qwFrameCount; ++i) {
		poPage = kPhysToPage(qwPhys + (i * PAGE_SIZE));
		if(NULL != poPage) {
			poPage->qwFlags |= PG_SLAB;
			poPage->pvPrivate = poSlab;
		}
	}

	kListAdd(&(poSlab->stLink), &(poCache->stPartial));
	++poCache->qwNumSlabs;
	return poSlab;
}


BOOL kInitializeSlab(void)
{
	static const QWORD vqSize[KMALLOC_CACHE_CNT] = {
		8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384
	};
	static const char* vpcName[KMALLOC_CACHE_CNT] = {
		"kmalloc-8", "kmalloc-16", "kmalloc-32", "kmalloc-64",
		"kmalloc-128", "kmalloc-256", "kmalloc-512", "kmalloc-1024",
		"kmalloc-2048", "kmalloc-4096", "kmalloc-8192", "kmalloc-16384"
	};
	int i;

	kMemSet(gvstCacheTable, 0, sizeof(gvstCacheTable));
	g_bSlabReady = TRUE;

	for(i=0; i<KMALLOC_CACHE_CNT; ++i) {
		gvpoKmallocCache[i] = kKmemCacheCreate(vpcName[i], vqSize[i], 8);
		if(NULL == gvpoKmallocCache[i]) {
			g_bSlabReady = FALSE;
			return FALSE;
		}
	}

	return TRUE;
}


kmem_cache_t* kKmemCacheCreate(const char* pcName, QWORD qwSize, QWORD qwAlign)
{
	kmem_cache_t* poCache = NULL;
	QWORD qwSlabSize, qwHeader;
	int i, iOrder;

	if((0 == qwSize) || (FALSE == g_bSlabReady)) {
		return NULL;
	}
	if(qwAlign < 8) {
		qwAlign = 8;
	}

	for(i=0; i<SLAB_MAX_CACHE; ++i) {
		if(FALSE == gvstCacheTable[i].bUsed) {
			poCache = &(gvstCacheTable[i]);
			break;
		}
	}
	if(NULL == poCache) {
		return NULL;
	}

	kMemSet(poCache, 0, sizeof(kmem_cache_t));
	for(i=0; (i<SLAB_NAME_LEN-1) && (0 != pcName[i]); ++i) {
		poCache->vcName[i] = pcName[i];
	}
	poCache->qwAlign = qwAlign;
	// 프리 리스트 인덱스를 객체 안에 넣으므로 최소 QWORD 하나는 되어야 한다
	poCache->qwObjSize = ALIGN_UP((qwSize < sizeof(QWORD)) ? sizeof(QWORD) : qwSize,
			qwAlign);

	// 객체가 최소 8개는 들어가도록 order를 키운다
	for(iOrder=0; iOrder<PMM_MAX_ORDER; ++iOrder) {
		qwSlabSize = (1UL << iOrder) * PAGE_SIZE;
		qwHeader = ALIGN_UP(sizeof(slab_t), qwAlign);
		if(qwSlabSize <= qwHeader) {
			continue;
		}
		if(((qwSlabSize - qwHeader) / poCache->qwObjSize) >= 8) {
			break;
		}
	}
	if(PMM_MAX_ORDER <= iOrder) {
		return NULL;
	}

	poCache->iOrder = iOrder;
	qwSlabSize = (1UL << iOrder) * PAGE_SIZE;
	qwHeader = ALIGN_UP(sizeof(slab_t), qwAlign);
	poCache->iObjsPerSlab = (int)((qwSlabSize - qwHeader) / poCache->qwObjSize);

	kListInit(&(poCache->stPartial));
	kListInit(&(poCache->stFull));
	poCache->bUsed = TRUE;
	return poCache;
}


void* kKmemCacheAlloc(kmem_cache_t* poCache)
{
	slab_t* poSlab;
	QWORD qwIndex, qwAddr;

	if(NULL == poCache) {
		return NULL;
	}

	if(TRUE == kListIsEmpty(&(poCache->stPartial))) {
		if(NULL == kSlabGrow(poCache)) {
			return NULL;
		}
	}
	poSlab = KCONTAINER_OF(poCache->stPartial.poNext, slab_t, stLink);

	qwIndex = poSlab->qwFirstFree;
	if(SLAB_NO_FREE == qwIndex) {
		return NULL;
	}
	// 호출자에게 주는 주소는 가상이어야 한다. 내부 계산은 물리 기준이므로
	// 오프셋을 구한 뒤 direct map으로 옮긴다
	qwAddr = (QWORD)__va(poSlab->qwPhysBase + kSlabObjOffset(poSlab, qwIndex));
	poSlab->qwFirstFree = *(QWORD*)qwAddr;
	--poSlab->qwFreeCount;
	++poCache->qwNumActive;

	// 다 썼으면 full로 옮긴다
	if(0 == poSlab->qwFreeCount) {
		kListDel(&(poSlab->stLink));
		kListAdd(&(poSlab->stLink), &(poCache->stFull));
	}

	kMemSet((void*)qwAddr, 0, (int)poCache->qwObjSize);
	return (void*)qwAddr;
}


void kKmemCacheFree(kmem_cache_t* poCache, void* pvObj)
{
	slab_t* poSlab;
	page_t* poPage;
	QWORD qwAddr;
	QWORD qwIndex, qwHeader;
	BOOL bWasFull;

	if((NULL == poCache) || (NULL == pvObj)) {
		return;
	}

	qwAddr = __pa(pvObj);
	poPage = kPhysToPage(qwAddr);
	if((NULL == poPage) || (0 == (poPage->qwFlags & PG_SLAB))) {
		return;
	}
	poSlab = (slab_t*)poPage->pvPrivate;		// kSlabGrow가 넣은 direct map 주소
	if((NULL == poSlab) || (poSlab->poCache != poCache)) {
		return;
	}

	qwHeader = ALIGN_UP(sizeof(slab_t), poCache->qwAlign);
	// qwAddr은 위에서 __pa()로 정규화했으므로 qwPhysBase와 같은 물리 기준이다
	qwIndex = (qwAddr - poSlab->qwPhysBase - qwHeader) / poCache->qwObjSize;
	if(qwIndex >= (QWORD)poCache->iObjsPerSlab) {
		return;
	}

#if SLAB_DEBUG
	kMemSet(__va(qwAddr), SLAB_POISON_FREE, (int)poCache->qwObjSize);
#endif

	bWasFull = (0 == poSlab->qwFreeCount) ? TRUE : FALSE;
	*(QWORD*)__va(qwAddr) = poSlab->qwFirstFree;
	poSlab->qwFirstFree = qwIndex;
	++poSlab->qwFreeCount;
	--poCache->qwNumActive;

	if(TRUE == bWasFull) {
		kListDel(&(poSlab->stLink));
		kListAdd(&(poSlab->stLink), &(poCache->stPartial));
	}
}


void* kmalloc(QWORD qwSize)
{
	int i;
	QWORD qwPhys;
	page_t* poPage;
	int iOrder;

	if(0 == qwSize) {
		return NULL;
	}

	for(i=0; i<KMALLOC_CACHE_CNT; ++i) {
		if(qwSize <= (8UL << i)) {
			return kKmemCacheAlloc(gvpoKmallocCache[i]);
		}
	}

	// 캐시 최대치를 넘으면 프레임을 직접 준다
	for(iOrder=0; iOrder<PMM_MAX_ORDER; ++iOrder) {
		if(qwSize <= ((1UL << iOrder) * PAGE_SIZE)) {
			break;
		}
	}
	if(PMM_MAX_ORDER <= iOrder) {
		return NULL;
	}

	qwPhys = kAllocPages(iOrder);
	if(0 == qwPhys) {
		return NULL;
	}
	poPage = kPhysToPage(qwPhys);
	if(NULL != poPage) {
		poPage->iOrder = iOrder;
		poPage->pvPrivate = NULL;		// PG_SLAB이 없으므로 kfree가 구분한다
	}
	return __va(qwPhys);
}


// 크기를 몰라도 된다. 프레임의 page_t가 소유자를 알고 있다
void kfree(void* pvAddr)
{
	page_t* poPage;

	if(NULL == pvAddr) {
		return;
	}

	poPage = kPhysToPage(__pa(pvAddr));
	if(NULL == poPage) {
		return;
	}

	if(poPage->qwFlags & PG_SLAB) {
		kKmemCacheFree(((slab_t*)poPage->pvPrivate)->poCache, pvAddr);
	}
	else {
		kFreePages(__pa(pvAddr), poPage->iOrder);
	}
}


void kPrintSlabInfo(void)
{
	char vcActive[24], vcSlabs[24], vcSize[24];
	int i;

	kPrintf("  name             size active slabs\n");
	for(i=0; i<SLAB_MAX_CACHE; ++i) {
		if(FALSE == gvstCacheTable[i].bUsed) {
			continue;
		}
		kUIToDecString(gvstCacheTable[i].qwObjSize, vcSize);
		kUIToDecString(gvstCacheTable[i].qwNumActive, vcActive);
		kUIToDecString(gvstCacheTable[i].qwNumSlabs, vcSlabs);
		kPrintf("  %s", gvstCacheTable[i].vcName);
		kPrintf("\t %s %s %s (%d/slab)\n", vcSize, vcActive, vcSlabs,
				gvstCacheTable[i].iObjsPerSlab);
	}
}
