#include "LinkedList.h"

void InitList(AxLink *ListHead)
{
    ListHead->Next = ListHead->Prev = ListHead;
}

void InsertListHead(AxLink *Link, AxLink *ListHead)
{
    AxLink *Next = ListHead->Next;

    Link->Next = Next;
    Link->Prev = ListHead;
    Next->Prev = Link;
    ListHead->Next = Link;
}

void InsertListTail(AxLink *Link, AxLink *ListHead)
{
    AxLink *Prev = ListHead->Prev;

    Link->Next = ListHead;
    Link->Prev = Prev;
    Prev->Next = Link;
    ListHead->Prev = Link;
}

bool IsListEmpty(AxLink *ListHead)
{
    return(ListHead->Next == ListHead);
}

AxLink *RemoveListHead(AxLink *ListHead)
{
    if (IsListEmpty(ListHead)) {
        return ListHead;
    }

    AxLink *Entry = ListHead->Next;
    AxLink *Next = Entry->Next;
    ListHead->Next = Next;
    Next->Prev = ListHead;

    return(Entry);
}

AxLink *RemoveListTail(AxLink *ListHead)
{
    if (IsListEmpty(ListHead)) {
        return ListHead;
    }

    AxLink *Entry = ListHead->Prev;
    AxLink *Prev = Entry->Prev;
    ListHead->Prev = Prev;
    Prev->Next = ListHead;

    return(Entry);
}

bool RemoveListLink(AxLink *Link)
{
    if (IsListEmpty(Link)) {
        return true;
    }

    AxLink *Next = Link->Next;
    AxLink *Prev = Link->Prev;
    Prev->Next = Next;
    Next->Prev = Prev;

    return(Next == Prev);
}

void AppendListTail(AxLink *ListToAppend, AxLink *ListHead)
{
    if (!ListHead || !ListToAppend) {
        return;
    }

    AxLink *ListEnd = ListHead->Prev;
    ListHead->Prev->Next = ListToAppend;
    ListHead->Prev = ListToAppend->Prev;
    ListToAppend->Prev->Next = ListHead;
    ListToAppend->Prev = ListEnd;
}