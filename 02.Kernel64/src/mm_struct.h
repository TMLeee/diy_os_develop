/*
 * mm_struct.h
 *
 *  Created on: 2026. 8. 1.
 *      Author: Macbook_pro
 *
 *  프로세스 주소공간. 리눅스 mm_struct / vm_area_struct에 대응
 */

#ifndef __MM_STRUCT_H__
#define __MM_STRUCT_H__

#include "types.h"
#include "mm.h"


// vm_area_t.qwFlags
#define VM_READ			0x0001
#define VM_WRITE		0x0002
#define VM_EXEC			0x0004
#define VM_GROWSDOWN	0x0008		// 스택. 아래로 자란다

// 유저 주소공간의 상한. PML4[0..255]가 유저 절반이다
#define USER_VA_END		0x0000800000000000UL

// 유저 스택은 유저 절반 꼭대기에 둔다
#define USER_STACK_TOP	0x0000700000000000UL
#define USER_STACK_SIZE	(16 * PAGE_SIZE)


// 한 구간 [qwStart, qwEnd). 시작 주소 오름차순으로 정렬해 둔다
typedef struct kVmAreaStruct {
	QWORD					qwStart;
	QWORD					qwEnd;
	QWORD					qwFlags;
	struct kVmAreaStruct*	poNext;
}vm_area_t;


typedef struct kMmStruct {
	QWORD		qwPML4;			// CR3에 넣을 물리주소
	vm_area_t*	poVmaList;		// 정렬된 단일 연결 리스트
	QWORD		qwCodeStart;
	QWORD		qwCodeEnd;
	QWORD		qwBrk;			// 힙 꼭대기
	int			iVmaCount;
}mm_t;


mm_t* kMmCreate(void);
void kMmDestroy(mm_t* poMm);

vm_area_t* kVmaCreate(mm_t* poMm, QWORD qwStart, QWORD qwEnd, QWORD qwFlags);
vm_area_t* kVmaFind(mm_t* poMm, QWORD qwAddr);

// VMA 플래그를 PTE 플래그로 옮기고 US를 붙인다
BOOL kMmMapPage(mm_t* poMm, QWORD qwVirtAddr, QWORD qwPhysAddr, QWORD qwVmFlags);
QWORD kMmVmToPteFlags(QWORD qwVmFlags);

QWORD kGetMmCount(void);

#endif
