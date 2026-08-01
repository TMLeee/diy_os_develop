/*
 * memmap.c
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  EntryPoint.s가 수집한 E820 테이블을 검증/정렬/병합해서 제공
 */

#include "memmap.h"
#include "console.h"
#include "paging.h"
#include "utility.h"


static E820Entry_t gvstMemMap[E820_MAX_ENTRIES];
static int g_iMemMapCount = 0;
static QWORD g_qwUsableSize = 0;
static QWORD g_qwHighestUsable = 0;


const char* kGetE820TypeName(DWORD dwType)
{
	switch(dwType) {
		case E820_TYPE_USABLE:			return "USABLE";
		case E820_TYPE_RESERVED:		return "RESERVED";
		case E820_TYPE_ACPI_RECLAIM:	return "ACPI-RECLAIM";
		case E820_TYPE_ACPI_NVS:		return "ACPI-NVS";
		case E820_TYPE_BAD:				return "BAD-RAM";
		default:						return "UNKNOWN";
	}
}


// qwBase 기준 오름차순 정렬(엔트리 수가 128 이하라 삽입 정렬로 충분)
static void kSortMemMap(void)
{
	int i, j;
	E820Entry_t stTmp;

	for(i=1; i<g_iMemMapCount; ++i) {
		stTmp = gvstMemMap[i];
		for(j=i-1; (0 <= j) && (gvstMemMap[j].qwBase > stTmp.qwBase); --j) {
			gvstMemMap[j+1] = gvstMemMap[j];
		}
		gvstMemMap[j+1] = stTmp;
	}
}


// 같은 타입으로 인접/중첩된 구간을 하나로 합친다
static void kMergeMemMap(void)
{
	int iSrc, iDst;
	QWORD qwEnd, qwNextEnd;

	if(0 == g_iMemMapCount) {
		return;
	}

	iDst = 0;
	for(iSrc=1; iSrc<g_iMemMapCount; ++iSrc) {
		qwEnd = gvstMemMap[iDst].qwBase + gvstMemMap[iDst].qwLength;

		if((gvstMemMap[iSrc].dwType == gvstMemMap[iDst].dwType) &&
		   (gvstMemMap[iSrc].qwBase <= qwEnd)) {
			qwNextEnd = gvstMemMap[iSrc].qwBase + gvstMemMap[iSrc].qwLength;
			if(qwNextEnd > qwEnd) {
				gvstMemMap[iDst].qwLength = qwNextEnd - gvstMemMap[iDst].qwBase;
			}
		}
		else {
			++iDst;
			gvstMemMap[iDst] = gvstMemMap[iSrc];
		}
	}
	g_iMemMapCount = iDst + 1;
}


BOOL kInitializeMemoryMap(void)
{
	BootInfo_t* poBootInfo = (BootInfo_t*)__va(BOOTINFO_ADDR);
	int i, iCount;
	QWORD qwEnd;

	g_iMemMapCount = 0;
	g_qwUsableSize = 0;
	g_qwHighestUsable = 0;

	if(BOOTINFO_MAGIC != poBootInfo->dwMagic) {
		return FALSE;
	}

	iCount = poBootInfo->wE820Count;
	if((0 == iCount) || (E820_MAX_ENTRIES < iCount)) {
		return FALSE;
	}

	// 길이 0 엔트리를 걸러내면서 복사
	for(i=0; i<iCount; ++i) {
		if(0 == poBootInfo->vstE820[i].qwLength) {
			continue;
		}
		gvstMemMap[g_iMemMapCount] = poBootInfo->vstE820[i];
		++g_iMemMapCount;
	}

	if(0 == g_iMemMapCount) {
		return FALSE;
	}

	kSortMemMap();
	kMergeMemMap();

	for(i=0; i<g_iMemMapCount; ++i) {
		if(E820_TYPE_USABLE != gvstMemMap[i].dwType) {
			continue;
		}
		g_qwUsableSize += gvstMemMap[i].qwLength;
		qwEnd = gvstMemMap[i].qwBase + gvstMemMap[i].qwLength;
		if(qwEnd > g_qwHighestUsable) {
			g_qwHighestUsable = qwEnd;
		}
	}

	return TRUE;
}


int kGetE820Count(void)
{
	return g_iMemMapCount;
}


const E820Entry_t* kGetE820Entry(int iIndex)
{
	if((iIndex < 0) || (g_iMemMapCount <= iIndex)) {
		return NULL;
	}
	return &(gvstMemMap[iIndex]);
}


QWORD kGetUsableMemorySize(void)
{
	return g_qwUsableSize;
}


QWORD kGetHighestUsableAddr(void)
{
	return g_qwHighestUsable;
}


// [qwBase, qwBase+qwSize)가 USABLE 구간 하나에 온전히 들어가는지
BOOL kIsUsableRegion(QWORD qwBase, QWORD qwSize)
{
	int i;
	QWORD qwEnd = qwBase + qwSize;

	for(i=0; i<g_iMemMapCount; ++i) {
		if(E820_TYPE_USABLE != gvstMemMap[i].dwType) {
			continue;
		}
		if((gvstMemMap[i].qwBase <= qwBase) &&
		   (qwEnd <= (gvstMemMap[i].qwBase + gvstMemMap[i].qwLength))) {
			return TRUE;
		}
	}
	return FALSE;
}


void kPrintMemoryMap(void)
{
	int i;
	const E820Entry_t* poEntry;
	char vcBase[17], vcEnd[17], vcSize[24];

	if(0 == g_iMemMapCount) {
		kPrintf("Memory map not available\n");
		return;
	}

	kPrintf("  base          end           type\n");
	for(i=0; i<g_iMemMapCount; ++i) {
		poEntry = &(gvstMemMap[i]);
		kToHexString(poEntry->qwBase, vcBase, 12);
		kToHexString(poEntry->qwBase + poEntry->qwLength, vcEnd, 12);
		kPrintf("  %s  %s  %s\n", vcBase, vcEnd, kGetE820TypeName(poEntry->dwType));
	}

	kUIToDecString(g_qwUsableSize / 0x100000, vcSize);
	kToHexString(g_qwHighestUsable, vcEnd, 12);
	kPrintf("entries=%d usable=%sMB highest=%s\n", g_iMemMapCount, vcSize, vcEnd);
}
