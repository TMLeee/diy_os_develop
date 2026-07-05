#include "list.h"

void kInitializeList(List_t *poList)
{
    poList->iItemSize   = 0;
    poList->poHead      = NULL;
    poList->poTail      = NULL;
}


int kGetListCount(const List_t *poList)
{
    return poList->iItemSize;
}


void kAddListToTail(List_t *poList, void* poItem)
{
    ListLink_t *poLink;

    poLink = (ListLink_t*)poItem;
    poLink->poNext = NULL;

    if(NULL == poList->poHead) {
        poList->poHead = poItem;
        poList->poTail = poItem;
        poList->iItemSize = 1;
        return;
    }

    poLink = (ListLink_t*)poList->poTail;
    poLink->poNext = poItem;

    poList->poTail = poItem;
    ++poList->iItemSize;
}


void kAddListToHead(List_t *poList, void* poItem)
{
    ListLink_t *poLink;

    poLink = (ListLink_t*)poItem;
    poLink->poNext = poList->poHead;

    if(NULL == poList->poHead) {
        poList->poHead = poItem;
        poList->poTail = poItem;
        poList->iItemSize = 1;
        return;
    }

    poList->poHead = poItem;
    ++poList->iItemSize;
}


void* kRemoveList(List_t* poList, QWORD qwID)
{
    ListLink_t *poLink;
    ListLink_t *poPrevLink;

    poPrevLink = (ListLink_t*)poList->poHead;
    for(poLink = poPrevLink;
        NULL != poLink;
        poLink = poLink->poNext)
    {
        if(qwID == poLink->qwID) {
            if((poLink == poList->poHead) &&
                (poList == poList->poTail)) {
                    poList->poHead  = NULL;
                    poList->poTail  = NULL;
            }
            else if(poLink == poList->poHead) {
                poList->poHead = poLink->poNext;
            }
            else if(poLink == poList->poTail) {
                poList->poTail = poPrevLink;
            }
            else {
                poPrevLink->poNext = poLink->poNext;
            }

            --poList->iItemSize;
            return poLink;
        }
        poPrevLink = poLink;
    }

    return NULL;
}


void* kRemoveListFromHead(List_t *poList)
{
    ListLink_t *poLink;

    if(0 == poList->iItemSize) {
        return NULL;
    }

    poLink = (ListLink_t*)poList->poHead;
    return kRemoveList(poList, poLink->qwID);
}


void* kRemoveListFromTail(List_t *poList)
{
    ListLink_t *poLink;
    if(0 == poList->iItemSize) {
        return NULL;
    }

    poLink = (ListLink_t*)poList->poTail;
    return kRemoveList(poList, poLink->qwID);
}


void* kFindList(const List_t *poList, QWORD qwID)
{
    ListLink_t *poLink;

    for(poLink = (ListLink_t*)poList->poHead;
        NULL != poLink;
        poLink = poLink->poNext)
    {
        if(poLink->qwID == qwID) {
            return poLink;
        }
    }
    return NULL;
}


void* kGetHeadFromList(const List_t *poList)
{
    return poList->poHead;
}


void* kGetTailFromList(const List_t *poList)
{
    return poList->poTail;
}


void* kGetNextFromList(const List_t *poList, void* poCurrent)
{
    ListLink_t *poLink;
    poLink = (ListLink_t*)poCurrent;
    return poLink->poNext;
}
