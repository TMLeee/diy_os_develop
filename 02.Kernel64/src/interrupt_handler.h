/*
 * interrupt_handler.h
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_INTERRUPT_HANDLER_H_
#define __02_KERNEL64_SRC_INTERRUPT_HANDLER_H_

#include "types.h"

void kCommonExceptionHandler(int iVectorNum, QWORD qwErrCode);
void kCommonInterruptHandler(int iVectorNum);
void kKeyboardHandler(int iVectorNum);



#endif /* 02_KERNEL64_SRC_INTERRUPT_HANDLER_H_ */
