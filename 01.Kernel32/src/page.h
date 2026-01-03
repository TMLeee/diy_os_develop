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

#pragma pack(push, 1)

typedef struct kPageTblEntStruct {
	DWORD dwAttrLowAddr;
	DWORD dwUppAddr;
}PWL4Ent_t, PDPTEnt_t, PDEnt_t, PTEnt_t;

#pragma pack(pop)

void kInitPageTbl(void);
void kSetPageEntry(PTEnt_t *poEntry, DWORD dwUppAddr, DWORD dwLowAddr, DWORD dwLowFlag, DWORD dwUppFlag);



#endif /* 01_KERNEL32_SRC_PAGE_H_ */
