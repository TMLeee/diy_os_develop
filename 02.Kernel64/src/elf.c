/*
 * elf.c
 *
 *  Created on: 2026. 8. 2.
 *      Author: Macbook_pro
 */


#include "elf.h"
#include "paging.h"
#include "pmm.h"
#include "utility.h"
#include "console.h"


BOOL kElfIsValid(const BYTE* pbImage, QWORD qwSize)
{
	const Elf64Ehdr_t* poEhdr = (const Elf64Ehdr_t*)pbImage;

	if((NULL == pbImage) || (sizeof(Elf64Ehdr_t) > qwSize)) {
		return FALSE;
	}
	if((ELF_MAGIC0 != poEhdr->vucIdent[0]) || (ELF_MAGIC1 != poEhdr->vucIdent[1]) ||
	   (ELF_MAGIC2 != poEhdr->vucIdent[2]) || (ELF_MAGIC3 != poEhdr->vucIdent[3])) {
		return FALSE;
	}
	if((ELFCLASS64 != poEhdr->vucIdent[4]) || (ELFDATA2LSB != poEhdr->vucIdent[5])) {
		return FALSE;
	}
	if((ET_EXEC != poEhdr->wType) || (EM_X86_64 != poEhdr->wMachine)) {
		return FALSE;
	}

	// 프로그램 헤더 테이블이 이미지 안에 있는지
	if(poEhdr->qwPhoff + ((QWORD)poEhdr->wPhnum * poEhdr->wPhentsize) > qwSize) {
		return FALSE;
	}
	return TRUE;
}


static QWORD kElfPhdrToVmFlags(DWORD dwFlags)
{
	QWORD qwVm = 0;

	if(0 != (dwFlags & PF_R)) {
		qwVm |= VM_READ;
	}
	if(0 != (dwFlags & PF_W)) {
		qwVm |= VM_WRITE;
	}
	if(0 != (dwFlags & PF_X)) {
		qwVm |= VM_EXEC;
	}
	return qwVm;
}


// 세그먼트 하나를 올린다. 파일에 있는 부분은 지금 복사해야 하므로 프레임을
// 바로 잡고, filesz를 넘는 .bss 쪽은 건드릴 때 폴트로 채우게 둔다
static BOOL kElfLoadSegment(mm_t* poMm, const BYTE* pbImage,
							const Elf64Phdr_t* poPhdr)
{
	QWORD qwStart = PAGE_ALIGN_DOWN(poPhdr->qwVaddr);
	QWORD qwEnd = PAGE_ALIGN_UP(poPhdr->qwVaddr + poPhdr->qwMemsz);
	QWORD qwFileEnd = poPhdr->qwVaddr + poPhdr->qwFilesz;
	QWORD qwVmFlags = kElfPhdrToVmFlags(poPhdr->dwFlags);
	QWORD qwVirtAddr, qwPhys, qwCopyFrom, qwCopyTo, qwOffsetInPage;

	if(NULL == kVmaCreate(poMm, qwStart, qwEnd, qwVmFlags)) {
		return FALSE;
	}

	for(qwVirtAddr = qwStart; qwVirtAddr < qwEnd; qwVirtAddr += PAGE_SIZE) {
		// 이 페이지가 파일 내용과 겹치지 않으면 순수 .bss다. 지연 할당에 맡긴다
		if(qwVirtAddr >= qwFileEnd) {
			continue;
		}

		qwPhys = kAllocPage();
		if(0 == qwPhys) {
			return FALSE;
		}

		// 페이지 앞뒤로 파일 밖 영역이 남을 수 있다. 통째로 0으로 깔고 덮는다
		kMemSet(__va(qwPhys), 0, PAGE_SIZE);

		qwCopyFrom = (qwVirtAddr > poPhdr->qwVaddr) ? qwVirtAddr : poPhdr->qwVaddr;
		qwCopyTo = ((qwVirtAddr + PAGE_SIZE) < qwFileEnd)
					? (qwVirtAddr + PAGE_SIZE) : qwFileEnd;
		qwOffsetInPage = qwCopyFrom - qwVirtAddr;

		kMemCpy((BYTE*)__va(qwPhys) + qwOffsetInPage,
				pbImage + poPhdr->qwOffset + (qwCopyFrom - poPhdr->qwVaddr),
				(int)(qwCopyTo - qwCopyFrom));

		if(FALSE == kMmMapPage(poMm, qwVirtAddr, qwPhys, qwVmFlags)) {
			kFreePage(qwPhys);
			return FALSE;
		}
	}
	return TRUE;
}


QWORD kElfLoad(mm_t* poMm, const BYTE* pbImage, QWORD qwSize)
{
	const Elf64Ehdr_t* poEhdr = (const Elf64Ehdr_t*)pbImage;
	const Elf64Phdr_t* poPhdr;
	QWORD qwLowest = USER_VA_END, qwHighest = 0;
	int i;

	if((NULL == poMm) || (FALSE == kElfIsValid(pbImage, qwSize))) {
		return 0;
	}

	for(i=0; i<poEhdr->wPhnum; ++i) {
		poPhdr = (const Elf64Phdr_t*)(pbImage + poEhdr->qwPhoff +
										((QWORD)i * poEhdr->wPhentsize));

		if((PT_LOAD != poPhdr->dwType) || (0 == poPhdr->qwMemsz)) {
			continue;
		}

		// 유저 절반 밖으로 나가는 세그먼트는 거절한다. 안 그러면 커널 주소에
		// 유저 페이지를 올리게 된다
		if((USER_VA_END <= poPhdr->qwVaddr) ||
		   (USER_VA_END <= (poPhdr->qwVaddr + poPhdr->qwMemsz)) ||
		   (poPhdr->qwFilesz > poPhdr->qwMemsz) ||
		   ((poPhdr->qwOffset + poPhdr->qwFilesz) > qwSize)) {
			return 0;
		}

		if(FALSE == kElfLoadSegment(poMm, pbImage, poPhdr)) {
			return 0;
		}

		if(poPhdr->qwVaddr < qwLowest) {
			qwLowest = poPhdr->qwVaddr;
		}
		if((poPhdr->qwVaddr + poPhdr->qwMemsz) > qwHighest) {
			qwHighest = poPhdr->qwVaddr + poPhdr->qwMemsz;
		}
	}

	if(0 == qwHighest) {
		return 0;
	}

	poMm->qwCodeStart = qwLowest;
	poMm->qwCodeEnd = qwHighest;
	poMm->qwBrk = PAGE_ALIGN_UP(qwHighest);

	return poEhdr->qwEntry;
}
