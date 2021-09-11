#pragma once

#include "Foundation/Types.h"

// Interface for a circular intrusive doubly linked list.

#define ListEntry(address, type, field) ((type *)((uint8_t *)(address) - (uint64_t)(&((type *)0)->field)))
#define container_of(ptr, type, member) ((type *)((uint8_t *)ptr - offsetof(type, member)))

typedef struct AxLink
{
    // Points to the next entry in the list or to the list header if there is no next entry.
    struct AxLink *Next;

    // Points to the previous entry in the list or to the list header if there is no previous entry.
    struct AxLink *Prev;
} AxLink;

/**
 * Initializes a AxLink structure that represents the head of a circular, doubly linked list.
 * @param ListHead Pointer to the AxLink structure that represents the head of the list.
 */
void InitList(AxLink *ListHead);

/**
 * Inserts a link at the head of doubly linked list of AxLink structures.
 *
 * @desc Updates ListHead->Next to point to Link. It updates Link->Next to point to the old first link
 * in the list and sets Link->Prev to ListHead. The Prev member of the original first link is
 * also updated to point to Link.
 *
 * @param Link Pointer to the AxLink structure to be inserted.
 * @param ListHead Pointer to the AxLink structure that represents the head of the list.
 */
void InsertListHead(AxLink *Link, AxLink *ListHead);


/**
 * Inserts a link at the tail of doubly linked list of AxLink structures.
 * @param Link Pointer to the AxLink structure to be inserted.
 * @param ListHead Pointer to the AxLink structure that represents the head of the list.
 */
void InsertListTail(AxLink *Link, AxLink *ListHead);

/**
 * Removes a link from the beginning of a doubly linked list of AxLink structures.
 *
 * @desc Removes the first link from the list by setting ListHead->Next to point to the second link in the list.
 * The function sets the Prev member of the second link to ListHead. If the list is empty, this is a no-op.
 *
 * @return A pointer to the link that was the head of the list. If the list was empty, returns ListHead.
 */
AxLink *RemoveListHead(AxLink *ListHead);

/**
 * Removes a link from the end of a doubly linked list of AxLink structures.
 * @return A pointer to the link that was the tail of the list. If the list was empty, returns ListHead.
 */
AxLink *RemoveListTail(AxLink *ListHead);

/**
 * Removes an link from a doubly linked list of AxLink structures.
 *
 * @desc Removes the link by setting the Next member of the link before Link to point to the link after Link,
 * and the Prev member of the link after Link to point to the link before Link. The return value can 
 * be used to detect when the last link is removed from the list. An empty list consists of a list head
 * and no links.
 *
 * @param Link Pointer to the AxLink structure to be removed.
 * @return True if, after removal, the list is empty. Otherwise, returns False to indicate that the
 *         resulting list still contains one or more entries.
 */
bool RemoveListLink(AxLink *Link);

/**
 * Checks to see if a list is empty.
 * @param ListHead Pointer to the AxLink structure that represents the head of the list.
 * @return Returns True if there are no previous or next entries, otherwise false.
 */
bool IsListEmpty(AxLink *ListHead);

/**
 * Appends an doubly linked list of AxLink structures to the end of another.
 * @param ListHead Pointer to the AxLink structure that represents the head of the list.
 * @param ListToAppend Pointer to the first entry in the list to append to the ListHead.
 */
void AppendListTail(AxLink *ListToAppend, AxLink *ListHead);