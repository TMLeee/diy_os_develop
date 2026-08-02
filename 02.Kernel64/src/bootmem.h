/*
 * bootmem.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_BOOTMEM_H_
#define __02_KERNEL64_SRC_BOOTMEM_H_

#include "types.h"


BOOL kInitializeBootmem(void);
void* kBootmemAlloc(QWORD qwSize);
QWORD kBootmemGetStart(void);
QWORD kBootmemGetUsed(void);
QWORD kBootmemGetLimit(void);
void kBootmemFreeze(void);


#endif /* 02_KERNEL64_SRC_BOOTMEM_H_ */
