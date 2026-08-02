/*
 * memmap.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_MEMMAP_H_
#define __02_KERNEL64_SRC_MEMMAP_H_

#include "types.h"
#include "bootinfo.h"


BOOL kInitializeMemoryMap(void);
int kGetE820Count(void);
const E820Entry_t* kGetE820Entry(int iIndex);
QWORD kGetUsableMemorySize(void);
QWORD kGetHighestUsableAddr(void);
BOOL kIsUsableRegion(QWORD qwBase, QWORD qwSize);
const char* kGetE820TypeName(DWORD dwType);
void kPrintMemoryMap(void);


#endif /* 02_KERNEL64_SRC_MEMMAP_H_ */
