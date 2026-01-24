/*
 * queue.c
 *
 *  Created on: 2026. 1. 24.
 *      Author: Macbook_pro
 */


#include "queue.h"



void kInitializeQueue(QUEUE* poQueue, void* pvQueueBuf, int iMaxDataCnt, int iDataSize)
{
	// 큐 정보를 저장
	poQueue->iMaxDataCnt	= iMaxDataCnt;
	poQueue->iDataSize		= iDataSize;
	poQueue->pvQueueArr		= pvQueueBuf;

	// 큐 초기화
	poQueue->iPutIdx	= 0;
	poQueue->iGetIdx	= 0;
	poQueue->bLastOpPut	= FALSE;
}


BOOL kIsQueueFull(const QUEUE* poQueue)
{
	// 큐가 가득 찼는지 확인
	if((poQueue->iGetIdx == poQueue->iPutIdx) && (TRUE == poQueue->bLastOpPut)) {
		return TRUE;
	}

	return FALSE;
}


BOOL kIsQueueEmpty(const QUEUE* poQueue)
{
	if((poQueue->iGetIdx == poQueue->iPutIdx) && (FALSE == poQueue->bLastOpPut)) {
		return TRUE;
	}
	return FALSE;
}


BOOL kPutQueue(QUEUE* poQueue, const void* poData)
{
	// 큐가 찬 경우
	if(TRUE == kIsQueueFull(poQueue)) {
		return FALSE;
	}

	kMemCpy((char*)poQueue->pvQueueArr + (poQueue->iDataSize * poQueue->iPutIdx),
			poData, poQueue->iDataSize);

	poQueue->iPutIdx 	= (poQueue->iPutIdx + 1) % poQueue->iMaxDataCnt;
	poQueue->bLastOpPut	= TRUE;
	return TRUE;
}


BOOL kGetQueue(QUEUE* poQueue, void* poData)
{
	// 큐가 빈 경우
	if(TRUE == kIsQueueEmpty(poQueue)) {
		return FALSE;
	}

	kMemCpy(poData, (char*)poQueue->pvQueueArr + (poQueue->iDataSize * poQueue->iGetIdx), poQueue->iDataSize);

	poQueue->iGetIdx	= (poQueue->iGetIdx + 1) % poQueue->iMaxDataCnt;
	poQueue->bLastOpPut	= FALSE;
	return TRUE;
}
