#ifndef __LIST_H_
#define __LIST_H_

#include "types.h"

#pragma pack (push, 1)

// 리스트 자료구조
typedef struct kListLinkStruct {
    void* poNext;
    QWORD qwID;
} ListLink_t;

// 리스트 데이터 자료구조
struct kListItemExpStruct {
    ListLink_t stLink;

    int iData1;
    char cData2;
};

// 리스트 관리 자료구조
typedef struct kListMngStruct {
    int iItemSize;

    void *poHead;
    void *poTail;
}List_t;

#pragma pack (pop)

void kInitializeList(List_t *poList);
int kGetListCount(const List_t *poList);
void kAddListToTail(List_t *poList, void* poItem);
void kAddListToHead(List_t *poList, void* poItem);
void* kRemoveList(List_t* poList, QWORD qwID);
void* kRemoveListFromHead(List_t *poList);
void* kRemoveListFromTail(List_t *poList);
void* kFindList(const List_t *poList, QWORD qwID);
void* kGetHeadFromList(const List_t *poList);
void* kGetTailFromList(const List_t *poList);
void* kGetNextFromList(const List_t *poList, void* poCurrent);

#endif