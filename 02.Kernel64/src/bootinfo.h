/*
 * bootinfo.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  01.Kernel32/src/EntryPoint.s가 실모드에서 채워 넣는 부팅 정보
 */

#ifndef __02_KERNEL64_SRC_BOOTINFO_H_
#define __02_KERNEL64_SRC_BOOTINFO_H_

#include "types.h"

#define BOOTINFO_ADDR			0x7E00
#define BOOTINFO_MAGIC			0x46495442		// 'BTIF'
#define E820_MAX_ENTRIES		128

// E820 엔트리 타입
#define E820_TYPE_USABLE		1
#define E820_TYPE_RESERVED		2
#define E820_TYPE_ACPI_RECLAIM	3
#define E820_TYPE_ACPI_NVS		4
#define E820_TYPE_BAD			5

#pragma pack(push, 1)

typedef struct kE820EntryStruct {
	QWORD qwBase;
	QWORD qwLength;
	DWORD dwType;
	DWORD dwExtAttr;
}E820Entry_t;								// 24바이트

typedef struct kBootInfoStruct {
	DWORD dwMagic;					// 0x7E00
	WORD wE820Count;				// 0x7E04
	WORD wReserved;					// 0x7E06, EntryPoint.s가 엔트리를 0x7E08부터 쓴다
	E820Entry_t vstE820[E820_MAX_ENTRIES];	// 0x7E08
}BootInfo_t;

#pragma pack(pop)

#endif /* 02_KERNEL64_SRC_BOOTINFO_H_ */
