/*
 * klist.h
 *
 *  Created on: 2026. 7. 30.
 *      Author: Macbook_pro
 *
 *  리눅스식 침습적 양방향 순환 리스트.
 *  기존 list.c는 단방향이고 kRemoveList가 ID 매칭 O(n)이라 buddy의
 *  free_area[]처럼 임의 노드를 O(1)로 빼야 하는 곳에는 쓸 수 없다.
 */

#ifndef __02_KERNEL64_SRC_KLIST_H_
#define __02_KERNEL64_SRC_KLIST_H_

#include "types.h"

typedef struct kListHeadStruct {
	struct kListHeadStruct* poPrev;
	struct kListHeadStruct* poNext;
}KListHead_t;


// 구조체 멤버 포인터에서 그 구조체의 시작 주소를 구한다
#define KOFFSET_OF(type, member)		((QWORD)&(((type*)0)->member))
#define KCONTAINER_OF(ptr, type, member) \
			((type*)((QWORD)(ptr) - KOFFSET_OF(type, member)))

#define KLIST_FOR_EACH(pos, head) \
			for((pos) = (head)->poNext; (pos) != (head); (pos) = (pos)->poNext)


static inline void kListInit(KListHead_t* poHead)
{
	poHead->poPrev = poHead;
	poHead->poNext = poHead;
}


static inline BOOL kListIsEmpty(const KListHead_t* poHead)
{
	return (poHead->poNext == poHead) ? TRUE : FALSE;
}


static inline void kListAdd(KListHead_t* poNew, KListHead_t* poHead)
{
	poNew->poNext = poHead->poNext;
	poNew->poPrev = poHead;
	poHead->poNext->poPrev = poNew;
	poHead->poNext = poNew;
}


static inline void kListAddTail(KListHead_t* poNew, KListHead_t* poHead)
{
	poNew->poNext = poHead;
	poNew->poPrev = poHead->poPrev;
	poHead->poPrev->poNext = poNew;
	poHead->poPrev = poNew;
}


// 어떤 리스트에 속해 있든 O(1)로 제거
static inline void kListDel(KListHead_t* poEntry)
{
	poEntry->poPrev->poNext = poEntry->poNext;
	poEntry->poNext->poPrev = poEntry->poPrev;
	poEntry->poPrev = poEntry;
	poEntry->poNext = poEntry;
}


static inline KListHead_t* kListPopFront(KListHead_t* poHead)
{
	KListHead_t* poEntry;

	if(TRUE == kListIsEmpty(poHead)) {
		return NULL;
	}
	poEntry = poHead->poNext;
	kListDel(poEntry);
	return poEntry;
}


#endif /* 02_KERNEL64_SRC_KLIST_H_ */
