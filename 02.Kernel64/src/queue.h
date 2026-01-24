/*
 * queue.h
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_QUEUE_H_
#define __02_KERNEL64_SRC_QUEUE_H_

#include "types.h"

// Struct
#pragma pack (push, 1)

typedef struct kQueueManagerStruct
{
	int iDataSize;
	int iMaxDataCnt;

	void* pvQueueArr;
	int iPutIdx;
	int iGetIdx;

	BOOL bLastOpPut;
}QUEUE;

#pragma pack (pop)


void kInitializeQueue(QUEUE* poQueue, void* pvQueueBuf, int iMaxDataCnt, int iDataSize);
BOOL kIsQueueFull(const QUEUE* poQueue);
BOOL kIsQueueEmpty(const QUEUE* poQueue);
BOOL kPutQueue(QUEUE* poQueue, const void* poData);
BOOL kGetQueue(QUEUE* poQueue, void* poData);

#endif /* 02_KERNEL64_SRC_QUEUE_H_ */
