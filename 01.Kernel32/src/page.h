/*
 * page.h
 *
 *  Created on: 2026. 1. 1.
 *      Author: Macbook_pro
 */

#ifndef __01_KERNEL32_SRC_PAGE_H_
#define __01_KERNEL32_SRC_PAGE_H_

#include "types.h"

#define PAGE_FLAGS_P		0x00000001
#define PAGE_FLAGS_RW		0x00000002
#define PAGE_FLAGS_US		0x00000004
#define PAGE_FLAGS_PWT		0x00000008
#define PAGE_FLAGS_PCD		0x00000010
#define PAGE_FLAGS_A		0x00000020
#define PAGE_FLAGS_D		0x00000040
#define PAGE_FLAGS_PS		0x00000080
#define PAGE_FLAGS_G		0x00000100
#define PAGE_FLAGS_PAT		0x00001000
#define PAGE_FLAGS_EXB		0x80000000
#define PAGE_FLAGS_DEF		(PAGE_FLAGS_P | PAGE_FLAGS_RW)

#define PAGE_TBL_SIZE		0x1000
#define PAGE_MAX_ENT_COUNT	512
#define PAGE_DEF_SIZE		0x200000

// 부트 identity 맵. 예전에는 PD 64장(64GB)이 0x102000~0x141FFF를 전부 채워
// higher-half 창을 놓을 자리가 없었다. Kernel64가 실제 RAM 크기로 테이블을
// 다시 만들므로 부트 단계에는 16GB면 충분하다
#define PAGE_IDENT_PD_BASE	0x102000
#define PAGE_IDENT_PD_CNT	16					// 0x102000~0x111FFF, 16GB

// higher-half 창용 PDPT/PD. 위에서 비운 자리를 쓴다.
// 02.Kernel64/src/mm.h 의 KERNEL32_PAGETABLE_BASE/SIZE(0x100000,0x42000) 안이다
#define PAGE_HIGH_PDPT		0x112000
#define PAGE_HIGH_PD		0x113000

#pragma pack(push, 1)

typedef struct kPageTblEntStruct {
	DWORD dwAttrLowAddr;
	DWORD dwUppAddr;
}PWL4Ent_t, PDPTEnt_t, PDEnt_t, PTEnt_t;

#pragma pack(pop)

void kInitPageTbl(void);
void kSetPageEntry(PTEnt_t *poEntry, DWORD dwUppAddr, DWORD dwLowAddr, DWORD dwLowFlag, DWORD dwUppFlag);



#endif /* 01_KERNEL32_SRC_PAGE_H_ */
