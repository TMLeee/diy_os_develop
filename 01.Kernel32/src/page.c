/*
 * page.c
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#include "page.h"

void kInitPageTbl(void)
{
	PWL4Ent_t* poPWL4Ent;
	PDPTEnt_t* poPDPTEnt;
	PDEnt_t* poPDEnt;
	DWORD dwMapAddr;
	int i;

	// PML4 Table 생성
	poPWL4Ent = (PWL4Ent_t*)0x100000;
	kSetPageEntry( &(poPWL4Ent[0]), 0x00, 0x101000, PAGE_FLAGS_DEF, 0);
	for(i=1; i<PAGE_MAX_ENT_COUNT; ++i) {
		kSetPageEntry( &(poPWL4Ent[i]), 0, 0, 0, 0);
	}

	// Page Directory Pointer Table 생성
	poPDPTEnt = (PDPTEnt_t*)0x101000;
	for(i=0; i<64; ++i) {
		kSetPageEntry( &(poPDPTEnt[i]), 0, 0x102000 + (i * PAGE_TBL_SIZE), PAGE_FLAGS_DEF, 0);
	}
	for(i=64; i<PAGE_MAX_ENT_COUNT; ++i) {
		kSetPageEntry( &(poPDPTEnt[i]), 0, 0, 0, 0);
	}

	// Page Directory Table 생성
	poPDEnt = (PDEnt_t*)0x102000;
	dwMapAddr = 0;
	for(i=0; i<PAGE_MAX_ENT_COUNT * 64; ++i) {
		kSetPageEntry( &(poPDEnt[i]), (i * (PAGE_DEF_SIZE >> 20)) >> 12,
				dwMapAddr, PAGE_FLAGS_DEF | PAGE_FLAGS_PS, 0);
		dwMapAddr += PAGE_DEF_SIZE;
	}
}


void kSetPageEntry(PTEnt_t *poEntry, DWORD dwUppAddr, DWORD dwLowAddr, DWORD dwLowFlag, DWORD dwUppFlag)
{
	poEntry->dwAttrLowAddr = dwLowAddr | dwLowFlag;
	poEntry->dwUppAddr = (dwUppAddr & 0xFF) | dwUppFlag;
}
