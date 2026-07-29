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
