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
	PDPTEnt_t* poDPTEnt;
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
	for(i=0; i<PAGE_IDENT_PD_CNT; ++i) {
		kSetPageEntry( &(poPDPTEnt[i]), 0, PAGE_IDENT_PD_BASE + (i * PAGE_TBL_SIZE),
				PAGE_FLAGS_DEF, 0);
	}
	for(i=PAGE_IDENT_PD_CNT; i<PAGE_MAX_ENT_COUNT; ++i) {
		kSetPageEntry( &(poPDPTEnt[i]), 0, 0, 0, 0);
	}

	// Page Directory Table 생성
	poPDEnt = (PDEnt_t*)PAGE_IDENT_PD_BASE;
	dwMapAddr = 0;
	for(i=0; i<PAGE_MAX_ENT_COUNT * PAGE_IDENT_PD_CNT; ++i) {
		kSetPageEntry( &(poPDEnt[i]), (i * (PAGE_DEF_SIZE >> 20)) >> 12,
				dwMapAddr, PAGE_FLAGS_DEF | PAGE_FLAGS_PS, 0);
		dwMapAddr += PAGE_DEF_SIZE;
	}

	// higher-half 창: 0xFFFFFFFF80200000 -> 물리 0x200000
	//   PML4[511] -> PDPT[510] -> PD[1] (2MB 페이지)
	// Kernel64가 자기 테이블로 바꾸기 전까지 고주소에서 실행하기 위한 최소 매핑
	poPDPTEnt = (PDPTEnt_t*)PAGE_HIGH_PDPT;
	for(i=0; i<PAGE_MAX_ENT_COUNT; ++i) {
		kSetPageEntry( &(poPDPTEnt[i]), 0, 0, 0, 0);
	}
	kSetPageEntry( &(poPDPTEnt[510]), 0, PAGE_HIGH_PD, PAGE_FLAGS_DEF, 0);

	poPDEnt = (PDEnt_t*)PAGE_HIGH_PD;
	for(i=0; i<PAGE_MAX_ENT_COUNT; ++i) {
		kSetPageEntry( &(poPDEnt[i]), 0, 0, 0, 0);
	}
	kSetPageEntry( &(poPDEnt[1]), 0, 0x200000, PAGE_FLAGS_DEF | PAGE_FLAGS_PS, 0);

	kSetPageEntry( &(poPWL4Ent[511]), 0, PAGE_HIGH_PDPT, PAGE_FLAGS_DEF, 0);

	// direct map: 0xFFFF800000000000 -> 물리 0. PML4[256] -> PDPT[0..N] ->
	// identity가 쓰는 PD를 그대로 재사용한다(같은 물리 메모리, 같은 2MB 페이지)
	poDPTEnt = (PDPTEnt_t*)PAGE_DIRECT_PDPT;
	for(i=0; i<PAGE_IDENT_PD_CNT; ++i) {
		kSetPageEntry( &(poDPTEnt[i]), 0, PAGE_IDENT_PD_BASE + (i * PAGE_TBL_SIZE),
				PAGE_FLAGS_DEF, 0);
	}
	for(i=PAGE_IDENT_PD_CNT; i<PAGE_MAX_ENT_COUNT; ++i) {
		kSetPageEntry( &(poDPTEnt[i]), 0, 0, 0, 0);
	}

	kSetPageEntry( &(poPWL4Ent[256]), 0, PAGE_DIRECT_PDPT, PAGE_FLAGS_DEF, 0);
}


void kSetPageEntry(PTEnt_t *poEntry, DWORD dwUppAddr, DWORD dwLowAddr, DWORD dwLowFlag, DWORD dwUppFlag)
{
	poEntry->dwAttrLowAddr = dwLowAddr | dwLowFlag;
	poEntry->dwUppAddr = (dwUppAddr & 0xFF) | dwUppFlag;
}
