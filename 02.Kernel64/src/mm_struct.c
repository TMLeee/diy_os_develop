/*
 * mm_struct.c
 *
 *  Created on: 2026. 8. 1.
 *      Author: Macbook_pro
 */


#include "mm_struct.h"
#include "paging.h"
#include "pmm.h"
#include "slab.h"
#include "utility.h"
#include "assembly_utils.h"


static QWORD g_qwMmCount = 0;


QWORD kMmVmToPteFlags(QWORD qwVmFlags)
{
	QWORD qwFlags = PTE_US;

	if(0 != (qwVmFlags & VM_WRITE)) {
		qwFlags |= PTE_RW;
	}
	if((0 == (qwVmFlags & VM_EXEC)) && (TRUE == kIsNXSupported())) {
		qwFlags |= PTE_NX;
	}
	return qwFlags;
}


mm_t* kMmCreate(void)
{
	mm_t* poMm;
	QWORD qwFrame;
	pte_t* poNew;
	pte_t* poKernel;
	int i;

	poMm = (mm_t*)kmalloc(sizeof(mm_t));
	if(NULL == poMm) {
		return NULL;
	}

	qwFrame = kAllocPage();
	if(0 == qwFrame) {
		kfree(poMm);
		return NULL;
	}

	poNew = (pte_t*)__va(qwFrame);
	kMemSet(poNew, 0, PAGE_SIZE);

	// 커널 절반을 공유한다. 안 그러면 mov cr3 다음 명령에서 죽는다.
	// 엔트리가 나중에 생기면 반영되지 않아 kInitializePaging이 미리 만들어 둔다
	poKernel = (pte_t*)__va(PTE_ADDR(kGetKernelCR3()));
	for(i=256; i<512; ++i) {
		poNew[i] = poKernel[i];
	}

	poMm->qwPML4		= qwFrame;
	poMm->poVmaList		= NULL;
	poMm->qwCodeStart	= 0;
	poMm->qwCodeEnd		= 0;
	poMm->qwBrk			= 0;
	poMm->iVmaCount		= 0;

	++g_qwMmCount;
	return poMm;
}


// 유저 절반만 반납한다. 커널 절반은 공유물이라 i가 256에서 멈춘다
static void kFreeUserTables(QWORD qwPML4)
{
	pte_t* poPML4 = (pte_t*)__va(PTE_ADDR(qwPML4));
	pte_t *poPDPT, *poPD, *poPT;
	int i, j, k, l;

	for(i=0; i<256; ++i) {
		if(0 == (poPML4[i] & PTE_P)) {
			continue;
		}
		poPDPT = (pte_t*)__va(PTE_ADDR(poPML4[i]));

		for(j=0; j<512; ++j) {
			if(0 == (poPDPT[j] & PTE_P)) {
				continue;
			}
			// 유저 매핑은 4KB로만 만든다. PS 엔트리를 테이블로 착각하면
			// 엉뚱한 프레임을 반납하게 되므로 건너뛴다
			if(0 != (poPDPT[j] & PTE_PS)) {
				continue;
			}
			poPD = (pte_t*)__va(PTE_ADDR(poPDPT[j]));

			for(k=0; k<512; ++k) {
				if(0 == (poPD[k] & PTE_P)) {
					continue;
				}
				if(0 != (poPD[k] & PTE_PS)) {
					continue;
				}
				poPT = (pte_t*)__va(PTE_ADDR(poPD[k]));

				for(l=0; l<512; ++l) {
					if(0 != (poPT[l] & PTE_P)) {
						// fork로 공유 중일 수 있다. 마지막 참조에서만 반납된다
						kPagePut(PTE_ADDR(poPT[l]));
					}
				}
				kFreePage(PTE_ADDR(poPD[k]));
			}
			kFreePage(PTE_ADDR(poPDPT[j]));
		}
		kFreePage(PTE_ADDR(poPML4[i]));
	}

	kFreePage(PTE_ADDR(qwPML4));
}


void kMmDestroy(mm_t* poMm)
{
	vm_area_t *poVma, *poNext;

	if(NULL == poMm) {
		return;
	}

	// 지금 돌고 있는 주소공간이면 먼저 빠져나온다
	if(PTE_ADDR(kReadCR3()) == PTE_ADDR(poMm->qwPML4)) {
		kWriteCR3(kGetKernelCR3());
	}

	kFreeUserTables(poMm->qwPML4);

	for(poVma = poMm->poVmaList; NULL != poVma; poVma = poNext) {
		poNext = poVma->poNext;
		kfree(poVma);
	}

	kfree(poMm);
	--g_qwMmCount;
}


// 시작 주소 오름차순 삽입. 겹치면 거절 - 폴트 때 어느 권한인지 알 수 없다
vm_area_t* kVmaCreate(mm_t* poMm, QWORD qwStart, QWORD qwEnd, QWORD qwFlags)
{
	vm_area_t *poNew, *poCur, *poPrev;

	if((NULL == poMm) || (qwEnd <= qwStart) || (USER_VA_END < qwEnd)) {
		return NULL;
	}

	qwStart = PAGE_ALIGN_DOWN(qwStart);
	qwEnd = PAGE_ALIGN_UP(qwEnd);

	poPrev = NULL;
	for(poCur = poMm->poVmaList; NULL != poCur; poCur = poCur->poNext) {
		if((qwStart < poCur->qwEnd) && (poCur->qwStart < qwEnd)) {
			return NULL;
		}
		if(poCur->qwStart > qwStart) {
			break;
		}
		poPrev = poCur;
	}

	poNew = (vm_area_t*)kmalloc(sizeof(vm_area_t));
	if(NULL == poNew) {
		return NULL;
	}

	poNew->qwStart	= qwStart;
	poNew->qwEnd	= qwEnd;
	poNew->qwFlags	= qwFlags;
	poNew->poNext	= poCur;

	if(NULL == poPrev) {
		poMm->poVmaList = poNew;
	}
	else {
		poPrev->poNext = poNew;
	}

	++poMm->iVmaCount;
	return poNew;
}


vm_area_t* kVmaFind(mm_t* poMm, QWORD qwAddr)
{
	vm_area_t* poVma;

	if(NULL == poMm) {
		return NULL;
	}

	for(poVma = poMm->poVmaList; NULL != poVma; poVma = poVma->poNext) {
		if(qwAddr < poVma->qwStart) {
			return NULL;
		}
		if(qwAddr < poVma->qwEnd) {
			return poVma;
		}
	}
	return NULL;
}


// 리프를 공유하고 양쪽 다 RW를 뺀다.
// VMA는 VM_WRITE인데 PTE에 RW가 없다 = COW, 라는 규약을 쓴다
static BOOL kMmCopyPtes(mm_t* poDst, QWORD qwSrcPML4)
{
	pte_t* poPML4 = (pte_t*)__va(PTE_ADDR(qwSrcPML4));
	pte_t *poPDPT, *poPD, *poPT;
	QWORD qwVirtAddr, qwFlags;
	int i, j, k, l;

	for(i=0; i<256; ++i) {
		if(0 == (poPML4[i] & PTE_P)) {
			continue;
		}
		poPDPT = (pte_t*)__va(PTE_ADDR(poPML4[i]));

		for(j=0; j<512; ++j) {
			if((0 == (poPDPT[j] & PTE_P)) || (0 != (poPDPT[j] & PTE_PS))) {
				continue;
			}
			poPD = (pte_t*)__va(PTE_ADDR(poPDPT[j]));

			for(k=0; k<512; ++k) {
				if((0 == (poPD[k] & PTE_P)) || (0 != (poPD[k] & PTE_PS))) {
					continue;
				}
				poPT = (pte_t*)__va(PTE_ADDR(poPD[k]));

				for(l=0; l<512; ++l) {
					if(0 == (poPT[l] & PTE_P)) {
						continue;
					}

					qwVirtAddr = ((QWORD)i << 39) | ((QWORD)j << 30) |
								 ((QWORD)k << 21) | ((QWORD)l << 12);

					qwFlags = poPT[l] & (PTE_US | PTE_NX);

					if(FALSE == kMapPage(poDst->qwPML4, qwVirtAddr,
										PTE_ADDR(poPT[l]), qwFlags)) {
						return FALSE;
					}
					kPageGet(PTE_ADDR(poPT[l]));

					// 부모도 같이 강등한다. 안 하면 부모가 자식 메모리를 고친다
					poPT[l] &= ~PTE_RW;
				}
			}
		}
	}
	return TRUE;
}


mm_t* kMmCopy(mm_t* poSrc)
{
	mm_t* poNew;
	vm_area_t* poVma;

	if(NULL == poSrc) {
		return NULL;
	}

	poNew = kMmCreate();
	if(NULL == poNew) {
		return NULL;
	}

	for(poVma = poSrc->poVmaList; NULL != poVma; poVma = poVma->poNext) {
		if(NULL == kVmaCreate(poNew, poVma->qwStart, poVma->qwEnd, poVma->qwFlags)) {
			kMmDestroy(poNew);
			return NULL;
		}
	}

	if(FALSE == kMmCopyPtes(poNew, poSrc->qwPML4)) {
		kMmDestroy(poNew);
		return NULL;
	}

	poNew->qwCodeStart	= poSrc->qwCodeStart;
	poNew->qwCodeEnd	= poSrc->qwCodeEnd;
	poNew->qwBrk		= poSrc->qwBrk;

	// 부모 PTE를 고쳤으므로 TLB를 버린다
	kWriteCR3(kReadCR3());
	return poNew;
}


BOOL kMmMapPage(mm_t* poMm, QWORD qwVirtAddr, QWORD qwPhysAddr, QWORD qwVmFlags)
{
	if((NULL == poMm) || (USER_VA_END <= qwVirtAddr)) {
		return FALSE;
	}
	return kMapPage(poMm->qwPML4, qwVirtAddr, qwPhysAddr,
					kMmVmToPteFlags(qwVmFlags));
}


QWORD kGetMmCount(void)
{
	return g_qwMmCount;
}
