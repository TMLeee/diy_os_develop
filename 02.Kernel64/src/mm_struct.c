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
	QWORD qwFlags = PTE_US;		// 유저 페이지는 US 없이는 의미가 없다

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

	// 커널 절반(PML4[256..511])을 통째로 공유한다. direct map, vmalloc, 커널
	// 이미지 창이 모든 주소공간에서 같은 자리에 있어야 CR3를 바꾼 직후에도
	// 커널 코드가 계속 실행되고 스택이 유효하다.
	// 커널 절반의 PML4 엔트리는 부팅 중에 전부 만들어지고 이후 늘지 않으므로
	// 생성 시점에 한 번 복사하면 충분하다
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


// 유저 절반에 매달린 프레임과 테이블을 전부 반납한다. 커널 절반은 공유물이라
// 손대면 안 된다 - 그래서 i는 256에서 멈춘다
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
						kFreePage(PTE_ADDR(poPT[l]));
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

	// 돌고 있는 주소공간을 없애면 다음 명령어에서 죽는다
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


// 시작 주소 오름차순을 유지하며 삽입한다. 겹치면 거절 - 겹친 VMA를 허용하면
// 폴트 핸들러가 어느 쪽 권한을 써야 할지 알 수 없다
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
		// 정렬돼 있으므로 시작이 넘어가면 더 볼 필요가 없다
		if(qwAddr < poVma->qwStart) {
			return NULL;
		}
		if(qwAddr < poVma->qwEnd) {
			return poVma;
		}
	}
	return NULL;
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
