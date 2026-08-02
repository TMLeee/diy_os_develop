/*
 * slab.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 */

#ifndef __02_KERNEL64_SRC_SLAB_H_
#define __02_KERNEL64_SRC_SLAB_H_

#include "types.h"
#include "mm.h"

#define SLAB_NAME_LEN		16
#define SLAB_MAX_CACHE		32
#define SLAB_POISON_FREE	0x5A		// 해제된 객체를 채우는 값
#define SLAB_DEBUG			1

typedef struct kKmemCacheStruct {
	char		vcName[SLAB_NAME_LEN];
	QWORD		qwObjSize;			// 정렬까지 반영된 실제 간격
	QWORD		qwAlign;
	int			iOrder;				// slab 하나가 쓰는 프레임 order
	int			iObjsPerSlab;
	KListHead_t	stPartial;			// 여유 객체가 있는 slab
	KListHead_t	stFull;				// 꽉 찬 slab
	QWORD		qwNumActive;
	QWORD		qwNumSlabs;
	BOOL		bUsed;
}kmem_cache_t;


BOOL kInitializeSlab(void);
kmem_cache_t* kKmemCacheCreate(const char* pcName, QWORD qwSize, QWORD qwAlign);
void* kKmemCacheAlloc(kmem_cache_t* poCache);
void kKmemCacheFree(kmem_cache_t* poCache, void* pvObj);
void kPrintSlabInfo(void);

void* kmalloc(QWORD qwSize);
void kfree(void* pvAddr);


#endif /* 02_KERNEL64_SRC_SLAB_H_ */
