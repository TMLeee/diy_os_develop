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
#include "vmalloc.h"


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


// 고주소 커널 창과 direct map을 각각 되돌린다. 저주소는 유저 VA라 그대로다
QWORD __pa(const void* pvVirtAddr)
{
	QWORD qwVirtAddr = (QWORD)pvVirtAddr;

	// elf_x86_64.x 가 AT(0x200000) 으로 VMA = KERNEL_VMA + PA 를 만든다.
	// VMA 에 이미 0x200000 이 들어 있으므로 KERNEL_PHYS_BASE 를 또 더하면 안 된다
	if(qwVirtAddr >= KERNEL_VMA) {
		return qwVirtAddr - KERNEL_VMA;
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

	// 테이블 엔트리는 물리주소다. 걷는 쪽은 direct map으로 본다
	poTable = (pte_t*)__va(PTE_ADDR(qwCR3));
	qwEntry = poTable[PML4_INDEX(qwVirtAddr)];
	pvqEntry[0] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}

	poTable = (pte_t*)__va(PTE_ADDR(qwEntry));
	qwEntry = poTable[PDPT_INDEX(qwVirtAddr)];
	pvqEntry[1] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}
	if(qwEntry & PTE_PS) {
		return PG_LEVEL_1G;
	}

	poTable = (pte_t*)__va(PTE_ADDR(qwEntry));
	qwEntry = poTable[PD_INDEX(qwVirtAddr)];
	pvqEntry[2] = qwEntry;
	if(0 == (qwEntry & PTE_P)) {
		return PG_LEVEL_NONE;
	}
	if(qwEntry & PTE_PS) {
		return PG_LEVEL_2M;
	}

	poTable = (pte_t*)__va(PTE_ADDR(qwEntry));
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


// 중간 테이블을 따라가며 필요하면 새로 만든다. 반환값은 direct map 주소다.
// 중간 레벨은 항상 P|RW로 두고 제약은 리프 PTE에만 건다. x86-64는 네 레벨의
// 권한을 AND하므로 중간에서 RW를 빼면 그 아래 전부가 읽기 전용이 된다.
// US만 요청대로 전파하고, 이미 있는 엔트리는 US를 올려 준다(리눅스와 같다)
static pte_t* kGetNextLevel(pte_t* poTable, QWORD qwIndex, BOOL bAlloc, QWORD qwUS)
{
	QWORD qwFrame;

	qwUS &= PTE_US;

	if(0 == (poTable[qwIndex] & PTE_P)) {
		if(FALSE == bAlloc) {
			return NULL;
		}
		qwFrame = kAllocPage();
		if(0 == qwFrame) {
			return NULL;
		}
		// 엔트리에는 물리주소가 들어가고(CPU가 걷는다), 내용을 지우는 것은
		// direct map을 통해 한다
		kMemSet(__va(qwFrame), 0, PAGE_SIZE);
		poTable[qwIndex] = qwFrame | PTE_P | PTE_RW | qwUS;
	}
	else if(poTable[qwIndex] & PTE_PS) {
		// 이미 2MB/1GB 페이지로 잡혀 있으면 여기서는 쪼개지 않는다
		return NULL;
	}
	else {
		// 커널 매핑이 먼저 만들어 둔 테이블 아래에 유저 페이지가 들어오는 경우.
		// 리프에 US가 없으면 여전히 커널 전용이므로 안전하다
		poTable[qwIndex] |= qwUS;
	}

	return (pte_t*)__va(PTE_ADDR(poTable[qwIndex]));
}


BOOL kMapPage(QWORD qwCR3, QWORD qwVirtAddr, QWORD qwPhysAddr, QWORD qwFlags)
{
	pte_t* poTable = (pte_t*)__va(PTE_ADDR(qwCR3));

	poTable = kGetNextLevel(poTable, PML4_INDEX(qwVirtAddr), TRUE, qwFlags);
	if(NULL == poTable) return FALSE;
	poTable = kGetNextLevel(poTable, PDPT_INDEX(qwVirtAddr), TRUE, qwFlags);
	if(NULL == poTable) return FALSE;
	poTable = kGetNextLevel(poTable, PD_INDEX(qwVirtAddr), TRUE, qwFlags);
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
	pte_t* poTable = (pte_t*)__va(PTE_ADDR(qwCR3));

	poTable = kGetNextLevel(poTable, PML4_INDEX(qwVirtAddr), FALSE, 0);
	if(NULL == poTable) return;
	poTable = kGetNextLevel(poTable, PDPT_INDEX(qwVirtAddr), FALSE, 0);
	if(NULL == poTable) return;
	poTable = kGetNextLevel(poTable, PD_INDEX(qwVirtAddr), FALSE, 0);
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
		poTable = (pte_t*)__va(PTE_ADDR(qwCR3));
		poTable = kGetNextLevel(poTable, PML4_INDEX(qwVirtAddr + qwOffset), TRUE, qwFlags);
		if(NULL == poTable) return FALSE;
		poPD = kGetNextLevel(poTable, PDPT_INDEX(qwVirtAddr + qwOffset), TRUE, qwFlags);
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
	QWORD qwPTFrame, qwBase, qwPhys, qwFlags;
	QWORD qwTextEndPhys, qwRodataEndPhys;
	pte_t* poPT;
	pte_t* poTable;
	pte_t* poPD;
	int i;

	// 섹션 심볼은 이제 가상주소다. 물리 오프셋과 비교하려면 __pa()를 거쳐야 한다
	qwTextEndPhys = __pa(__text_end);
	qwRodataEndPhys = __pa(__rodata_end);

	// 커널 이미지가 2MB 하나를 넘어가면 이 로직으로는 부족하다
	if(__pa(__kernel_end) > (KERNEL_PHYS_BASE + PAGE_SIZE_2M)) {
		return FALSE;
	}

	qwPTFrame = kAllocPage();
	if(0 == qwPTFrame) {
		return FALSE;
	}
	poPT = (pte_t*)__va(qwPTFrame);

	// 코드가 실제로 실행되는 별칭은 고주소 쪽이다. 그쪽을 쪼갠다
	qwBase = KERNEL_VMA + KERNEL_PHYS_BASE;

	for(i=0; i<PAGE_ENTRY_COUNT; ++i) {
		qwPhys = KERNEL_PHYS_BASE + ((QWORD)i * PAGE_SIZE);

		if(qwPhys < qwTextEndPhys) {
			qwFlags = PTE_G;								// .text  RO + X
		}
		else if(qwPhys < qwRodataEndPhys) {
			qwFlags = PTE_G | qwNXFlag;						// .rodata RO + NX
		}
		else {
			qwFlags = PTE_RW | PTE_G | qwNXFlag;			// .data/.bss/여백 RW + NX
		}

		poPT[i] = PTE_ADDR(qwPhys) | qwFlags | PTE_P;
	}

	// 완성된 PT로 PDE를 교체
	poTable = (pte_t*)__va(PTE_ADDR(qwCR3));
	poTable = kGetNextLevel(poTable, PML4_INDEX(qwBase), TRUE, 0);
	if(NULL == poTable) return FALSE;
	poPD = kGetNextLevel(poTable, PDPT_INDEX(qwBase), TRUE, 0);
	if(NULL == poPD) return FALSE;

	poPD[PD_INDEX(qwBase)] = PTE_ADDR(qwPTFrame) | PTE_P | PTE_RW;
	return TRUE;
}


// PML4 엔트리 하나가 덮는 범위
#define PML4_SPAN	(1UL << 39)


// vmalloc 매핑은 부팅이 끝난 뒤 처음 생긴다. 그때 PML4 엔트리가 새로 만들어지면
// 이미 커널 절반을 복사해 간 주소공간에는 반영되지 않는다(리눅스가
// sync_global_pgds로 푸는 문제). 엔트리를 미리 다 만들어 두면 이후로는 PDPT
// 아래로만 자라고, 그 PDPT는 복사된 엔트리가 가리키는 같은 프레임이라 공유된다
static BOOL kPreallocKernelPML4(QWORD qwPML4)
{
	pte_t* poTable = (pte_t*)__va(PTE_ADDR(qwPML4));
	QWORD qwVA;

	for(qwVA = VMALLOC_START; qwVA < VMALLOC_END; qwVA += PML4_SPAN) {
		if(NULL == kGetNextLevel(poTable, PML4_INDEX(qwVA), TRUE, 0)) {
			return FALSE;
		}
	}
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
	kMemSet(__va(qwPML4), 0, PAGE_SIZE);

	// 1) RAM 전체를 PAGE_OFFSET에 매핑한다(direct map).
	//    identity 매핑은 만들지 않는다 - 저주소 전체가 비어야 유저 주소공간이
	//    거기에 들어갈 수 있고, 2MB 리프가 kMapPage()의 4KB 매핑을 막지 않는다
	if(FALSE == kMapRange2M(qwPML4, PAGE_OFFSET, 0, qwHighest,
				PTE_RW | PTE_G | qwNXFlag)) {
		return FALSE;
	}

	// 3) 커널 이미지의 고주소 창. NX를 걸면 안 된다 - CR3를 바꾸는 순간
	//    다음 명령어 인출이 바로 이 페이지에서 일어나므로 #PF -> 트리플폴트다.
	//    섹션별 권한은 바로 아래 kProtectKernelImage()가 4KB로 쪼개며 준다
	if(FALSE == kMapRange2M(qwPML4, KERNEL_VMA + KERNEL_PHYS_BASE,
				KERNEL_PHYS_BASE, PAGE_SIZE_2M, PTE_RW | PTE_G)) {
		return FALSE;
	}

	// 4) 그 2MB를 4KB로 쪼개고 섹션별 권한을 적용한다
	if(FALSE == kProtectKernelImage(qwPML4, qwNXFlag)) {
		return FALSE;
	}

	// 5) 커널 절반의 PML4 엔트리를 여기서 전부 확정한다
	if(FALSE == kPreallocKernelPML4(qwPML4)) {
		return FALSE;
	}

	g_qwKernelCR3 = qwPML4;
	kWriteCR3(qwPML4);

	// CR0.WP: 커널도 RO 페이지에 쓰지 못하게 한다. COW의 전제조건
	qwCR0 = kReadCR0();
	kWriteCR0(qwCR0 | CR0_WP);

	// CR3가 새 테이블을 가리키므로 Kernel32가 만든 264KB는 이제 죽은 메모리다.
	// 반드시 CR3 전환 뒤에 반납해야 한다
	kUnreserveRange(KERNEL32_PAGETABLE_BASE, KERNEL32_PAGETABLE_SIZE);

	return TRUE;
}
