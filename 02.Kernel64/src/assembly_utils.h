/*
 * assembly_utils.h
 *
 *  Created on: 2026. 1. 2.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_ASSEMBLY_UTILS_H_
#define __02_KERNEL64_SRC_ASSEMBLY_UTILS_H_

#include "types.h"

BYTE kInPortByte(WORD wPort);
void kOutPortByte(WORD wPort, BYTE ucData);


#endif /* 02_KERNEL64_SRC_ASSEMBLY_UTILS_H_ */
