/*
 * descriptor.h
 *
 *  Created on: 2026. 1. 3.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_DESCRIPTOR_H_
#define __02_KERNEL64_SRC_DESCRIPTOR_H_

#include "types.h"
#include "assembly_utils.h"


// GDT //

// BIT Define
#define GDT_TYPE_CODE			0x0A
#define GDT_TYPE_DATA			0x02
#define GDT_TYPE_TSS			0x09
#define GDT_FLAG_LOWER_S		0x10
#define GDT_FLAG_LOWER_DPL0		0x00
#define GDT_FLAG_LOWER_DPL1		0x20
#define GDT_FLAG_LOWER_DPL2		0x40
#define GDT_FLAG_LOWER_DPL3		0x60
#define GDT_FLAG_LOWER_P		0x80
#define GDT_FLAG_UPPER_L		0x20
#define GDT_FLAG_UPPER_DB		0x40
#define GDT_FLAG_UPPER_G		0x80

// Lower Flags //
#define GDT_FLAG_LOWER_KERNELCODE 	( GDT_TYPE_CODE | GDT_FLAG_LOWER_S | GDT_FLAG_LOWER_DPL0 | GDT_FLAG_LOWER_P )
#define GDT_FLAG_LOWER_KERNELDATA 	( GDT_TYPE_DATA | GDT_FLAG_LOWER_S | GDT_FLAG_LOWER_DPL0 | GDT_FLAG_LOWER_P )
#define GDT_FLAG_LOWER_TSS 			( GDT_FLAG_LOWER_DPL0 | GDT_FLAG_LOWER_P )
#define GDT_FLAG_LOWER_USERCODE 	( GDT_TYPE_CODE | GDT_FLAG_LOWER_S | GDT_FLAG_LOWER_DPL3 | GDT_FLAG_LOWER_P )
#define GDT_FLAG_LOWER_USERDATA 	( GDT_TYPE_DATA | GDT_FLAG_LOWER_S | GDT_FLAG_LOWER_DPL3 | GDT_FLAG_LOWER_P )

// Upper Flags //
#define GDT_FLAG_UPPER_CODE		( GDT_FLAG_UPPER_G | GDT_FLAG_UPPER_L )
#define GDT_FLAG_UPPER_DATA		( GDT_FLAG_UPPER_G | GDT_FLAG_UPPER_L )
#define GDT_FLAG_UPPER_TSS		( GDT_FLAG_UPPER_G )

// 세그먼트 디스크립션 오프셋
#define GDT_KERNEL_CODE_SEGMENT		0x08
#define GDT_KENNEL_DATA_SEGMENT		0x10
#define GDT_TSS_SEGMENT				0x18

// 기타 GDT 메크로
// GDT 시작 주소
// 페이지 테이블 영역: 1MB 부터 264KB
#define GDTR_START_ADDR		0x142000

// 8바이트 앤트리 갯수
// Null Descriptor, Kernel Code, Kernel Data
#define GDTR_8BYTE_ENT_SIZE		3

// 16바이트 앤트리 갯수
// TSS
#define GDTR_16BYTE_ENT_SIZE	1

// GDT 테이블 크기
#define GDT_TBL_SIZE		((sizeof(GDTEntry8_t) * GDTR_8BYTE_ENT_SIZE) + \
						 	 (sizeof(GDTEntry16_t) * GDTR_16BYTE_ENT_SIZE))
#define TSS_SEGMENT_SIZE	(sizeof(TSSSegment_t))


// IDT //
//BIT Define
#define IDT_TYPE_INTERRUPT	0x0E
#define IDT_TYPE_TRAP		0x0F
#define IDT_FLAG_DPL0		0x00
#define IDT_FLAG_DPL1		0x20
#define IDT_FLAG_DPL2		0x40
#define IDT_FLAG_DPL3		0x60
#define IDT_FLAG_P			0x80
#define IDT_FLAG_IST0		0
#define IDT_FLAG_IST1		1

// IDT 메크로
#define IDT_FLAG_KENREL		(IDT_FLAG_DPL0 | IDT_FLAG_P)
#define IDT_FLAG_USER		(IDT_FLAG_DPL3 | IDT_FLAG_P)

// IDT 관련 메크로
// IDT 엔크리 갯수
#define IDT_ENTRY_SIZE		100

// IDTR 시작 주소. TSS 세그먼트 다음에 위치
#define IDTR_START_ADDR		(GDTR_START_ADDR + sizeof(GDTR) + GDT_TBL_SIZE + TSS_SEGMENT_SIZE)

// IDT 테이블 시작 주소
#define IDT_START_ADDR		(IDTR_START_ADDR + sizeof(IDTR))

// IDT 테이블 크기
#define IDT_TBL_SIZE		(IDT_ENTRY_SIZE * sizeof(IDTEntry_t))

// IST 시작 주소
#define IST_START_ADDR		0x700000

// IST 크기
#define IST_SIZE			0x100000


// 구조체 자료구조
#pragma pack(push, 1)

// GDTR, IDTR
typedef struct kGDTRStruct {
	WORD wLimit;
	QWORD qwBaseAddr;
	WORD wPading;
	DWORD dwPading;
}GDTR, IDTR;

// 8바이트 GDT Entry
typedef struct kGDTEntry8Struct {
	WORD wLowLimit;
	WORD wLowBaseAddr;
	BYTE ucUppBaseAddr1;
	BYTE ucTypeAndLowFlag;		// 4Bit: Type, 1Bit: S, 2Bit: DPL, 1Bit: P
	BYTE ucUppLimitAndUppFlag;	// 4Bit: Seg Limit, 1Bit: AVL, L, D/B, G
	BYTE ucUppBaseAddr2;
}GDTEntry8_t;

// 16바이트 GDT Entry
typedef struct kGDTEntry16Struct {
	WORD wLowLimit;
	WORD wLowBaseAddr;
	BYTE ucMidBaseAddr1;
	BYTE ucTypeAndLowFlag;		// 4Bit: Type, 1Bit: 0, 2Bit: DPL, 1Bit P
	BYTE ucUppLimitAndUppFlag;	// 4Bit: Seg Limit, 1Bit: AVL, 0, 0, G
	BYTE ucMidBaseAddr2;
	DWORD dwUppBaseAddr;
	DWORD dwReserved;
}GDTEntry16_t;

// TSS Data 구조체
typedef struct kTSSDataStruct {
	DWORD dwReserved1;
	QWORD qwRsp[3];
	QWORD qwReserved2;
	QWORD qwIST[7];
	QWORD qwReserved3;
	WORD wReserved;
	WORD wIOMapBaseAddr;
}TSSSegment_t;

// IDT Gate Descriptor 구조체
typedef struct kIDTEntryStruct {
	WORD wLowBaseAddr;
	WORD wSegSelector;
	BYTE ucIST;			// 3Bit: IST, 5Bit: 0
	BYTE ucTypeAndFlag;	// 4Bit: Type, 1Bit: 0, 2Bit: DPL, 1Bit: P
	WORD wMidBaseAddr;
	DWORD dwUppBaseAddr;
	DWORD dwReserved;
}IDTEntry_t;

#pragma pack(pop)

void kInitGDTTableAndTSS(void);
void kSetGDTEntry8(GDTEntry8_t *poEntry, DWORD dwBaseAddr, DWORD dwLimit,
				BYTE ucUppFlag, BYTE ucLowFlag, BYTE ucType);
void kSetGDTEntry16(GDTEntry16_t *poEntry, QWORD qwBaseAddr, DWORD dwLimit,
				BYTE ucUppFlag, BYTE ucLowFlag, BYTE ucType);
void kInitTSSSegment(TSSSegment_t *poTSS);
void kInitTDTTable(void);
void kSetIDTEntry(IDTEntry_t *poEntry, void* pvHandler, WORD wSelector,
				BYTE ucIST, BYTE ucFlag, BYTE ucType);
void kDummyHandler(void);

#endif /* 02_KERNEL64_SRC_DESCRIPTOR_H_ */
