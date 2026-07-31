/*
 * vmalloc.c
 *
 *  Created on: 2026. 7. 31.
 *      Author: Macbook_pro
 *
 *  물리적으로 흩어진 프레임을 연속 가상주소로 묶는다.
 *  guard page 지원이 핵심 - 태스크 스택을 여기서 받으면 오버플로가
 *  조용한 메모리 손상 대신 #PF가 된다.
 */

#include "vmalloc.h"
#include "paging.h"
#include "pmm.h"
#include "slab.h"
#include "mm.h"
#include "console.h"
#include "utility.h"
#include "assembly_utils.h"


// 할당 하나의 기록. 해제할 때 프레임을 되돌리려면 목록이 필요하다
typedef struct kVmallocAreaStruct {
	KListHead_t	stLink;
	QWORD		qwVirtAddr;			// 매핑된 첫 페이지
	QWORD		qwTotalStart;		// guard 포함 시작
	int			iPageCount;
	int			iGuardBefore;
	int			iGuardAfter;
}vmalloc_area_t;

static KListHead_t g_stAreaList;
static kmem_cache_t* g_poAreaCache = NULL;
static QWORD g_qwNextFree = VMALLOC_START;
static QWORD g_qwMappedPages = 0;


BOOL kInitializeVmalloc(void)
{
	kListInit(&g_stAreaList);
	g_qwNextFree = VMALLOC_START;
	g_qwMappedPages = 0;

	g_poAreaCache = kKmemCacheCreate("vmalloc_area", sizeof(vmalloc_area_t), 8);
	return (NULL != g_poAreaCache) ? TRUE : FALSE;
}


BOOL kIsVmallocAddr(QWORD qwVirtAddr)
{
	return ((VMALLOC_START <= qwVirtAddr) && (qwVirtAddr < VMALLOC_END))
			? TRUE : FALSE;
}


// CR2가 guard page를 가리키는지. 스택 오버플로를 진단하는 데 쓴다
BOOL kIsVmallocGuardPage(QWORD qwVirtAddr)
{
	KListHead_t* poPos;
	vmalloc_area_t* poArea;
	QWORD qwPage = PAGE_ALIGN_DOWN(qwVirtAddr);

	if(FALSE == kIsVmallocAddr(qwVirtAddr)) {
		return FALSE;
	}

	KLIST_FOR_EACH(poPos, &g_stAreaList) {
		poArea = KCONTAINER_OF(poPos, vmalloc_area_t, stLink);

		// 앞쪽 guard
		if((poArea->qwTotalStart <= qwPage) && (qwPage < poArea->qwVirtAddr)) {
			return TRUE;
		}
		// 뒤쪽 guard
		if((qwPage >= (poArea->qwVirtAddr + ((QWORD)poArea->iPageCount * PAGE_SIZE))) &&
		   (qwPage < (poArea->qwVirtAddr
					  + ((QWORD)(poArea->iPageCount + poArea->iGuardAfter) * PAGE_SIZE)))) {
			return TRUE;
		}
	}
	return FALSE;
}


void* kVmapPages(int iPageCount, int iGuardBefore, int iGuardAfter)
{
	vmalloc_area_t* poArea;
	QWORD qwTotalStart, qwVirtAddr, qwPhys;
	int i, j;

	if((NULL == g_poAreaCache) || (iPageCount <= 0)) {
		return NULL;
	}
	if((iGuardBefore < 0) || (iGuardAfter < 0)) {
		return NULL;
	}

	qwTotalStart = g_qwNextFree;
	qwVirtAddr = qwTotalStart + ((QWORD)iGuardBefore * PAGE_SIZE);

	if((qwVirtAddr + ((QWORD)(iPageCount + iGuardAfter) * PAGE_SIZE)) >= VMALLOC_END) {
		return NULL;
	}

	poArea = (vmalloc_area_t*)kKmemCacheAlloc(g_poAreaCache);
	if(NULL == poArea) {
		return NULL;
	}

	// guard page는 매핑하지 않는다. 주소 공간만 비워 둔다
	for(i=0; i<iPageCount; ++i) {
		qwPhys = kAllocPage();
		if(0 == qwPhys) {
			// 여기까지 매핑한 것을 되돌린다
			for(j=0; j<i; ++j) {
				QWORD qwVA = qwVirtAddr + ((QWORD)j * PAGE_SIZE);
				QWORD qwPA = kVirtToPhys(kReadCR3(), qwVA);
				kUnmapPage(kReadCR3(), qwVA);
				if(0 != qwPA) {
					kFreePage(qwPA);
				}
			}
			kKmemCacheFree(g_poAreaCache, poArea);
			return NULL;
		}

		if(FALSE == kMapPage(kReadCR3(), qwVirtAddr + ((QWORD)i * PAGE_SIZE),
					qwPhys, PTE_RW | (kIsNXSupported() ? PTE_NX : 0))) {
			kFreePage(qwPhys);
			kKmemCacheFree(g_poAreaCache, poArea);
			return NULL;
		}
		kMemSet((void*)(qwVirtAddr + ((QWORD)i * PAGE_SIZE)), 0, PAGE_SIZE);
	}

	poArea->qwVirtAddr = qwVirtAddr;
	poArea->qwTotalStart = qwTotalStart;
	poArea->iPageCount = iPageCount;
	poArea->iGuardBefore = iGuardBefore;
	poArea->iGuardAfter = iGuardAfter;
	kListAdd(&(poArea->stLink), &g_stAreaList);

	g_qwNextFree = qwVirtAddr + ((QWORD)(iPageCount + iGuardAfter) * PAGE_SIZE);
	g_qwMappedPages += iPageCount;

	return (void*)qwVirtAddr;
}


void* kVmalloc(QWORD qwSize)
{
	int iPageCount;

	if(0 == qwSize) {
		return NULL;
	}
	iPageCount = (int)(PAGE_ALIGN_UP(qwSize) / PAGE_SIZE);

	// 넘침을 바로 잡을 수 있도록 뒤에 guard 한 장을 기본으로 붙인다
	return kVmapPages(iPageCount, 0, 1);
}


void kVfree(void* pvAddr)
{
	KListHead_t* poPos;
	vmalloc_area_t* poArea = NULL;
	QWORD qwVA, qwPA;
	int i;

	if(NULL == pvAddr) {
		return;
	}

	KLIST_FOR_EACH(poPos, &g_stAreaList) {
		if(KCONTAINER_OF(poPos, vmalloc_area_t, stLink)->qwVirtAddr == (QWORD)pvAddr) {
			poArea = KCONTAINER_OF(poPos, vmalloc_area_t, stLink);
			break;
		}
	}
	if(NULL == poArea) {
		return;
	}

	for(i=0; i<poArea->iPageCount; ++i) {
		qwVA = poArea->qwVirtAddr + ((QWORD)i * PAGE_SIZE);
		qwPA = kVirtToPhys(kReadCR3(), qwVA);
		kUnmapPage(kReadCR3(), qwVA);
		if(0 != qwPA) {
			kFreePage(qwPA);
		}
	}

	g_qwMappedPages -= poArea->iPageCount;
	kListDel(&(poArea->stLink));
	kKmemCacheFree(g_poAreaCache, poArea);
	// 주소 공간은 회수하지 않는다(단순 bump). 47TB라 당분간 문제없다
}


void kPrintVmallocInfo(void)
{
	KListHead_t* poPos;
	vmalloc_area_t* poArea;
	char vcHex[17], vcNum[24];
	int iCount = 0;

	KLIST_FOR_EACH(poPos, &g_stAreaList) {
		poArea = KCONTAINER_OF(poPos, vmalloc_area_t, stLink);
		kToHexString(poArea->qwVirtAddr, vcHex, 16);
		kPrintf("  %s %d pages guard %d/%d\n", vcHex, poArea->iPageCount,
				poArea->iGuardBefore, poArea->iGuardAfter);
		++iCount;
	}

	kUIToDecString(g_qwMappedPages, vcNum);
	kToHexString(g_qwNextFree, vcHex, 16);
	kPrintf("areas=%d mapped=%s pages next=%s\n", iCount, vcNum, vcHex);
}
