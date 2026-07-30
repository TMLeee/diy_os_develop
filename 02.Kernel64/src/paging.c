/*
 * paging.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#include "paging.h"
#include "console.h"
#include "utility.h"
#include "assembly_utils.h"
#include "pmm.h"
#include "memmap.h"


static QWORD g_qwKernelCR3 = 0;
static BOOL g_bNXSupported = FALSE;


QWORD kGetKernelCR3(void)
{
	return g_qwKernelCR3;
}


BOOL kIsNXSupported(void)
{
	return g_bNXSupported;
}


// direct map 영역이면 오프셋을 빼고, identity 영역이면 그대로 반환한다
QWORD __pa(const void* pvVirtAddr)
{
	QWORD qwVirtAddr = (QWORD)pvVirtAddr;

	if(qwVirtAddr >= KERNEL_VMA) {
		return qwVirtAddr - KERNEL_VMA + KERNEL_PHYS_BASE;
	}
	if(qwVirtAddr >= PAGE_OFFSET) {
		return qwVirtAddr - PAGE_OFFSET;
	}
	return qwVirtAddr;
}


const char* kGetPageLevelName(int iLevel)
{
	switch(iLevel) {
		case PG_LEVEL_1G:	return "1GB";
		case PG_LEVEL_2M:	return "2MB";
		case PG_LEVEL_4K:	return "4KB";
		default:			return "none";
	}
}


int kWalkPageTable(QWORD qwCR3, QWORD qwVirtAddr, pte_t* pvqEntry)
{
	pte_t* poTable;
	pte_t qwEntry;
	int i;

	for(i=0; i<4; ++i) {
		pvqEntry[i] = 0;
	}

	// identity 매핑이므로 테이블의 물리주소를 그대로 참조할 수 있다
	poTable = (pte_t*)PTE_ADDR(qwCR3);
	qwEntry = poTable[PML4_INDEX(qwVirtAddr)];
	pvqEntry[0] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}

	poTable = (pte_t*)PTE_ADDR(qwEntry);
	qwEntry = poTable[PDPT_INDEX(qwVirtAddr)];
	pvqEntry[1] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}
	if(qwEntry & PTE_PS) {
		return PG_LEVEL_1G;
	}

	poTable = (pte_t*)PTE_ADDR(qwEntry);
	qwEntry = poTable[PD_INDEX(qwVirtAddr)];
	pvqEntry[2] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}
	if(qwEntry & PTE_PS) {
		return PG_LEVEL_2M;
	}

	poTable = (pte_t*)PTE_ADDR(qwEntry);
	qwEntry = poTable[PT_INDEX(qwVirtAddr)];
	pvqEntry[3] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}

	return PG_LEVEL_4K;
}


QWORD kVirtToPhys(QWORD qwCR3, QWORD qwVirtAddr)
{
	pte_t vqEntry[4];
	int iLevel = kWalkPageTable(qwCR3, qwVirtAddr, vqEntry);

	switch(iLevel) {
		case PG_LEVEL_1G:
			return PTE_ADDR(vqEntry[1]) | (qwVirtAddr & 0x3FFFFFFF);
		case PG_LEVEL_2M:
			return PTE_ADDR(vqEntry[2]) | (qwVirtAddr & 0x1FFFFF);
		case PG_LEVEL_4K:
			return PTE_ADDR(vqEntry[3]) | (qwVirtAddr & 0xFFF);
		default:
			return 0;
	}
}


static void kPrintEntryFlags(pte_t qwEntry)
{
	kPrintf("%s%s%s%s%s%s",
			(qwEntry & PTE_P)  ? "P"  : "-",
			(qwEntry & PTE_RW) ? "W"  : "R",
			(qwEntry & PTE_US) ? "U"  : "S",
			(qwEntry & PTE_PS) ? "|PS" : "",
			(qwEntry & PTE_G)  ? "|G"  : "",
			(qwEntry & PTE_NX) ? "|NX" : "");
}


void kDumpPageWalk(QWORD qwCR3, QWORD qwVirtAddr)
{
	static const char* vpcLevelName[4] = {"PML4", "PDPT", "PD  ", "PT  "};
	static const int viShift[4] = {39, 30, 21, 12};
	pte_t vqEntry[4];
	int iLevel, i;
	QWORD qwPhys;
	char vcHex[17];

	iLevel = kWalkPageTable(qwCR3, qwVirtAddr, vqEntry);

	kToHexString(qwVirtAddr, vcHex, 16);
	kPrintf("VA %s  CR3=", vcHex);
	kToHexString(PTE_ADDR(qwCR3), vcHex, 12);
	kPrintf("%s\n", vcHex);

	for(i=0; i<4; ++i) {
		if((0 == vqEntry[i]) && (0 != i) && (0 == (vqEntry[i-1] & PTE_P))) {
			break;
		}
		kToHexString(vqEntry[i], vcHex, 16);
		kPrintf(" %s[%d] = %s ", vpcLevelName[i],
				(int)((qwVirtAddr >> viShift[i]) & 0x1FF), vcHex);
		kPrintEntryFlags(vqEntry[i]);
		kPrintf("\n");

		if(0 == (vqEntry[i] & PTE_P)) {
			break;
		}
		if((vqEntry[i] & PTE_PS) && (0 != i)) {
			break;
		}
	}

	qwPhys = kVirtToPhys(qwCR3, qwVirtAddr);
	if(PG_LEVEL_NONE == iLevel) {
		kPrintf(" -> NOT PRESENT\n");
	}
	else {
		kToHexString(qwPhys, vcHex, 12);
		kPrintf(" -> PA %s (%s page)\n", vcHex, kGetPageLevelName(iLevel));
	}
}


// 중간 테이블을 따라가며 필요하면 새로 만든다. 반환값은 다음 레벨 테이블의
// 가상주소(현재는 identity라 물리주소와 같다)
static pte_t* kGetNextLevel(pte_t* poTable, QWORD qwIndex, BOOL bAlloc)
{
	QWORD qwFrame;

	if(0 == (poTable[qwIndex] & PTE_P)) {
		if(FALSE == bAlloc) {
			return NULL;
		}
		qwFrame = kAllocPage();
		if(0 == qwFrame) {
			return NULL;
		}
		kMemSet((void*)qwFrame, 0, PAGE_SIZE);
		poTable[qwIndex] = qwFrame | PTE_P | PTE_RW;
	}
	else if(poTable[qwIndex] & PTE_PS) {
		// 이미 2MB/1GB 페이지로 잡혀 있으면 여기서는 쪼개지 않는다
		return NULL;
	}

	return (pte_t*)PTE_ADDR(poTable[qwIndex]);
}


BOOL kMapPage(QWORD qwCR3, QWORD qwVirtAddr, QWORD qwPhysAddr, QWORD qwFlags)
{
	pte_t* poTable = (pte_t*)PTE_ADDR(qwCR3);

	poTable = kGetNextLevel(poTable, PML4_INDEX(qwVirtAddr), TRUE);
	if(NULL == poTable) return FALSE;
	poTable = kGetNextLevel(poTable, PDPT_INDEX(qwVirtAddr), TRUE);
	if(NULL == poTable) return FALSE;
	poTable = kGetNextLevel(poTable, PD_INDEX(qwVirtAddr), TRUE);
	if(NULL == poTable) return FALSE;

	if(FALSE == g_bNXSupported) {
		qwFlags &= ~PTE_NX;
	}
	poTable[PT_INDEX(qwVirtAddr)] = PTE_ADDR(qwPhysAddr) | qwFlags | PTE_P;
	return TRUE;
}


BOOL kMapRange(QWORD qwCR3, QWORD qwVirtAddr, QWORD qwPhysAddr,
		QWORD qwSize, QWORD qwFlags)
{
	QWORD qwOffset;

	qwSize = PAGE_ALIGN_UP(qwSize);
	for(qwOffset=0; qwOffset<qwSize; qwOffset+=PAGE_SIZE) {
		if(FALSE == kMapPage(qwCR3, qwVirtAddr + qwOffset,
					qwPhysAddr + qwOffset, qwFlags)) {
			return FALSE;
		}
	}
	return TRUE;
}


void kUnmapPage(QWORD qwCR3, QWORD qwVirtAddr)
{
	pte_t* poTable = (pte_t*)PTE_ADDR(qwCR3);

	poTable = kGetNextLevel(poTable, PML4_INDEX(qwVirtAddr), FALSE);
	if(NULL == poTable) return;
	poTable = kGetNextLevel(poTable, PDPT_INDEX(qwVirtAddr), FALSE);
	if(NULL == poTable) return;
	poTable = kGetNextLevel(poTable, PD_INDEX(qwVirtAddr), FALSE);
	if(NULL == poTable) return;

	poTable[PT_INDEX(qwVirtAddr)] = 0;
	kInvlpg(qwVirtAddr);
}


// 2MB 페이지로 [qwVirtAddr, +qwSize)를 매핑한다. 중간 테이블만 4KB 프레임을
// 쓰고 리프는 PS=1이므로 64MB를 매핑해도 테이블이 몇 장 안 든다
static BOOL kMapRange2M(QWORD qwCR3, QWORD qwVirtAddr, QWORD qwPhysAddr,
		QWORD qwSize, QWORD qwFlags)
{
	pte_t* poTable;
	pte_t* poPD;
	QWORD qwOffset;

	qwSize = ALIGN_UP(qwSize, PAGE_SIZE_2M);
	for(qwOffset=0; qwOffset<qwSize; qwOffset+=PAGE_SIZE_2M) {
		poTable = (pte_t*)PTE_ADDR(qwCR3);
		poTable = kGetNextLevel(poTable, PML4_INDEX(qwVirtAddr + qwOffset), TRUE);
		if(NULL == poTable) return FALSE;
		poPD = kGetNextLevel(poTable, PDPT_INDEX(qwVirtAddr + qwOffset), TRUE);
		if(NULL == poPD) return FALSE;

		poPD[PD_INDEX(qwVirtAddr + qwOffset)] =
				PTE_ADDR(qwPhysAddr + qwOffset) | qwFlags | PTE_P | PTE_PS;
	}
	return TRUE;
}


// 커널 이미지를 덮고 있는 2MB PDE를 512엔트리 PT로 교체하고 섹션별 권한을 준다.
// PDE를 제자리에서 쪼갤 수는 없으므로 PT를 먼저 완성한 뒤 PDE를 한 번에 바꾼다
static BOOL kProtectKernelImage(QWORD qwCR3, QWORD qwNXFlag)
{
	QWORD qwPTFrame, qwBase, qwAddr, qwFlags;
	pte_t* poPT;
	pte_t* poTable;
	pte_t* poPD;
	int i;

	// 커널 이미지가 2MB 하나를 넘어가면 이 로직으로는 부족하다
	if((QWORD)__kernel_end > (KERNEL_PHYS_BASE + PAGE_SIZE_2M)) {
		return FALSE;
	}

	qwPTFrame = kAllocPage();
	if(0 == qwPTFrame) {
		return FALSE;
	}
	poPT = (pte_t*)qwPTFrame;
	qwBase = KERNEL_PHYS_BASE;

	for(i=0; i<PAGE_ENTRY_COUNT; ++i) {
		qwAddr = qwBase + ((QWORD)i * PAGE_SIZE);

		if(qwAddr < (QWORD)__text_end) {
			qwFlags = PTE_G;								// .text  RO + X
		}
		else if(qwAddr < (QWORD)__rodata_end) {
			qwFlags = PTE_G | qwNXFlag;						// .rodata RO + NX
		}
		else {
			qwFlags = PTE_RW | PTE_G | qwNXFlag;			// .data/.bss/여백 RW + NX
		}

		poPT[i] = PTE_ADDR(qwAddr) | qwFlags | PTE_P;
	}

	// 완성된 PT로 PDE를 교체
	poTable = (pte_t*)PTE_ADDR(qwCR3);
	poTable = kGetNextLevel(poTable, PML4_INDEX(qwBase), TRUE);
	if(NULL == poTable) return FALSE;
	poPD = kGetNextLevel(poTable, PDPT_INDEX(qwBase), TRUE);
	if(NULL == poPD) return FALSE;

	poPD[PD_INDEX(qwBase)] = PTE_ADDR(qwPTFrame) | PTE_P | PTE_RW;
	return TRUE;
}


BOOL kInitializePaging(void)
{
	QWORD qwPML4, qwHighest, qwNXFlag;
	DWORD dwEAX, dwEBX, dwECX, dwEDX;
	QWORD qwEFER, qwCR0;

	// NX 지원 확인. 지원하지 않는데 PTE_NX를 세우면 예약 비트 위반으로
	// 모든 매핑이 #PF가 된다
	kReadCPUID(0x80000001, &dwEAX, &dwEBX, &dwECX, &dwEDX);
	g_bNXSupported = (dwEDX & (1 << 20)) ? TRUE : FALSE;

	if(TRUE == g_bNXSupported) {
		kReadMSR(MSR_IA32_EFER, &qwEFER);
		kWriteMSR(MSR_IA32_EFER, qwEFER | EFER_NXE);
	}
	qwNXFlag = (TRUE == g_bNXSupported) ? PTE_NX : 0;

	qwHighest = kGetHighestUsableAddr();
	if(0 == qwHighest) {
		return FALSE;
	}

	qwPML4 = kAllocPage();
	if(0 == qwPML4) {
		return FALSE;
	}
	kMemSet((void*)qwPML4, 0, PAGE_SIZE);

	// 1) RAM 전체를 identity 매핑한다. 0xB8000, 0x700000 IST, 0x800000 TCB 풀 등
	//    하드코딩된 물리주소가 전부 그대로 동작해야 하므로 반드시 전 범위
	if(FALSE == kMapRange2M(qwPML4, 0, 0, qwHighest,
				PTE_RW | PTE_G | qwNXFlag)) {
		return FALSE;
	}

	// 2) 같은 물리 메모리를 PAGE_OFFSET에 한 번 더(direct map)
	if(FALSE == kMapRange2M(qwPML4, PAGE_OFFSET, 0, qwHighest,
				PTE_RW | PTE_G | qwNXFlag)) {
		return FALSE;
	}

	// 3) 커널 이미지가 든 2MB를 4KB로 쪼개고 섹션별 권한을 적용한다
	if(FALSE == kProtectKernelImage(qwPML4, qwNXFlag)) {
		return FALSE;
	}

	g_qwKernelCR3 = qwPML4;
	kWriteCR3(qwPML4);

	// CR0.WP: 커널도 RO 페이지에 쓰지 못하게 한다. COW의 전제조건
	qwCR0 = kReadCR0();
	kWriteCR0(qwCR0 | CR0_WP);

	return TRUE;
}
