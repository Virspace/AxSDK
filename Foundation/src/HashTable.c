#include "HashTable.h"
#include "Hash.h"
#include <string.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define DEFAULT_CAPACITY 16

// NOTE(mdeforge): Inspired by https://github.com/benhoyt/ht
// TODO(mdeforge): Evaluate would should be int32_t vs int64_t vs size_t

typedef struct HashEntry
{
    char *Key;              // Key is NULL if entry is empty
    void *Value;
} HashEntry;

typedef struct HashTable
{
    size_t Capacity;       // Size of the Entries array
    size_t Length;         // Number of HashEntry's in the hash table
    HashEntry *Entries;     // Hash entries
} HashTable;

static inline size_t FindIndex(uint64_t Hash, size_t BucketCount)
{
    return ((size_t)(Hash & (uint64_t)BucketCount - 1));
}

HashTable *CreateTable(size_t Capacity)
{
    HashTable *Table = malloc(sizeof(HashTable));
    if (Table == NULL) {
        return (NULL);
    }

    Table->Length = 0;
    Table->Capacity = (Capacity > 0) ? Capacity : DEFAULT_CAPACITY;
    Table->Entries = calloc(Table->Capacity, sizeof(HashEntry));

    if (Table->Entries == NULL)
    {
        free(Table);
        return (NULL);
    }

    return(Table);
}

void DestroyTable(HashTable *Table)
{
    // Free allocated keys
    for (size_t i = 0; i < Table->Capacity; i++)
    {
        if (Table->Entries[i].Key != NULL) {
            free((void *)Table->Entries[i].Key);
        }
    }

    // Free the table itself
    free(Table->Entries);
    free(Table);
}

/**
 * Searches for the key in the hash table.
 * @return NULL if it doesn't exist.
 */
void *HashTableSearch(HashTable *Table, const char *Key)
{
    if (Table == NULL) {
        return NULL;
    }

    // Get hash and index
    uint64_t Hash = HashStringFNV1a(Key, FNV1_64_INIT);
    size_t Index = FindIndex(Hash, Table->Capacity);

    // Loop until we find an empty entry
    while (Table->Entries[Index].Key != NULL)
    {
        if (strcmp(Table->Entries[Index].Key, Key) == 0) {
            return (Table->Entries[Index].Value); // Found key
        }

        // Key wasn't in slot, move to next entry (linear probe)
        Index++;

        // Wrap around if at the end
        if (Index >= Table->Capacity) {
            Index = 0;
        }
    }

    return(NULL);
}

const char *HashInsertEntry(HashEntry *Entries, size_t Capacity, const char *Key, void *Value, size_t *EntryLength)
{
    // Get hash and index
    uint64_t Hash = HashStringFNV1a(Key, FNV1_64_INIT);
    size_t Index = FindIndex(Hash, Capacity);

    // Loop until we find an empty entry
    while (Entries[Index].Key != NULL)
    {
        if (strcmp(Entries[Index].Key, Key) == 0)
        {
            // Found existing key, update value
            Entries[Index].Value = Value;
            return (Entries[Index].Key);
        }

        // Key wasn't here, move to next (linear probe)
        Index++;

        // Wrap around if at the end
        if (Index >= Capacity) {
            Index = 0;
        }
    }

    // Didn't find key, allocate and copy if needed, then insert it
    if (EntryLength != NULL)
    {
        Key = strdup(Key); // Note(mdeforge): This malloc's, don't forget to free!
        if (Key == NULL) {
            return (NULL);
        }

        (*EntryLength)++;
    }

    Entries[Index].Key = (char *)Key;
    Entries[Index].Value = Value;

    return (Key);
}

bool HashTableExpand(HashTable *Table)
{
    size_t NewCapacity = Table->Capacity * 2;

    HashEntry* NewEntries = calloc(NewCapacity, sizeof(HashEntry));
    if (NewEntries == NULL) {
        return (false);
    }

    // Iterate through entites, moving all non-empty ones to new table
    for (size_t i = 0; i < Table->Capacity; i++)
    {
        HashEntry Entry = Table->Entries[i];
        if (Entry.Key != NULL) {
            HashInsertEntry(NewEntries, NewCapacity, Entry.Key, Entry.Value, NULL);
        }
    }

    // Free old array and update the table
    free(Table->Entries);
    Table->Entries = NewEntries;
    Table->Capacity = NewCapacity;

    return (true);
}

const char *HashInsert(HashTable *Table, const char *Key, void *Value)
{
    //Assert(Value != NULL);
    if (!Table) { //  || !Value
        return (false);
    }

    // If length exceeds half of current capacity, expand it
    if (Table->Length >= Table->Capacity / 2)
    {
        if (!HashTableExpand(Table)) {
            return (NULL);
        }
    }

    // Set entry and update length
    return(HashInsertEntry(Table->Entries, Table->Capacity, Key, Value, &Table->Length));
}

size_t GetHashTableLength(HashTable *Table)
{
    return (Table->Length);
}

void *GetHashTableEntry(HashTable *Table, int32_t Index)
{
    // Loop until we find the entry at the index
    size_t NumEntry = 0;
    for (int32_t i = 0; i < Table->Capacity; ++i)
    {
        if (Table->Entries[i].Key != NULL)
        {
            if (NumEntry == Index) {
                return (Table->Entries[i].Value);
            }

            NumEntry++;
        }
    }

    return (NULL);
}

//void HashDelete(HashTable *Table, const char *Key)
//{
//    
//}