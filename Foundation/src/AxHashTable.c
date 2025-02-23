#include "AxHashTable.h"
#include "AxHash.h"
#include <stdlib.h>
#include <string.h>

/**
 * Inspired by:
 *  https://github.com/benhoyt/ht
 *  https://craftinginterpreters.com/hash-tables.html
 */

// TODO(mdeforge): Evaluate what should be int32_t vs int64_t vs size_t
// TODO(mdeforge): Use foundation allocators instead of malloc/calloc

#define MAX_LOAD 0.75

static const uint64_t AXON_HASH_UNUSED = 0xffffffffffffffffULL;    // -1
static const uint64_t AXON_HASH_TOMBSTONE = 0xfffffffffffffffeULL; // -2

typedef struct HashEntry
{
    char *Key;           // Key is NULL if entry is empty
    void *Value;
} HashEntry;

typedef struct AxHashTable
{
    size_t Capacity;     // Size of the Entries array
    size_t Size;       // Number of HashEntry's in the hash table
    HashEntry *Entries;  // Hash entries
} AxHashTable;

static inline size_t GrowCapacity(size_t Capacity)
{
    return ((Capacity < 8) ? 8 : Capacity * 2);
}

// Maps the key's hash to an index within the array's bounds
// TODO(mdeforge): This is a weak hash. Consider a mod by prime?
static inline size_t FindIndex(uint64_t Hash, size_t Capacity)
{
    return ((size_t)(Hash & (uint64_t)Capacity - 1));
}

static HashEntry *FindEntry(HashEntry *Entries, size_t Capacity, const char *Key)
{
    // Hash key
    uint64_t Hash = HashStringFNV1a(Key, FNV1A_64_INIT);

    // Find index of hash in table
    size_t Index = FindIndex(Hash, Capacity);
    HashEntry* Tombstone = NULL;

    // Starting at hash index, loop until we find an empty entry
    for (;;)
    {
        HashEntry *Entry = &Entries[Index];
        if (Entry->Key == NULL)
        {
            // We found a tombstone or an empty entry, figure out which one
            if (Entry->Value == (void *)AXON_HASH_TOMBSTONE)
            {
                // We found a tombstone, save it and keep going. We need to
                // keep going in order to make sure this is the last tombstone.
                // If we find an empty entry next loop we'll return the saved
                // tombstone so we can reuse the entry.
                if (Tombstone == NULL) {
                    Tombstone = Entry;
                }
            }
            else
            {
                // We found an empty entry, but if we just found a tombstone
                // return that instead so we can reuse the entry.
                return ((Tombstone == NULL) ? Entry : Tombstone);
            }
        }
        else if (strcmp(Entry->Key, Key) == 0)
        {
            // We found the key
            return (Entry);
        }

        // Key wasn't here, move to next (linear probe)
        // TODO(mdeforge): Consider quadradic probe
        Index = (Index + 1) % Capacity;
    }
}

static HashEntry *GetEntryAtIndex(AxHashTable *Table, size_t Index)
{
    HashEntry *Entry = NULL;
    size_t ValidEntries = 0;

    // Starting at the first index, loop until we find a real entry
    for (size_t i = 0; ValidEntries <= Index; i++)
    {
        Entry = &Table->Entries[i];
        if (Entry->Key == NULL) {
            continue;
        } else {
            ValidEntries++;
        }
    }

    return (Entry);
}

static AxHashTable *CreateTable()
{
    AxHashTable *Table = malloc(sizeof(AxHashTable));
    if (Table == NULL) {
        return (NULL);
    }

    Table->Size = 0;
    Table->Capacity = 0;
    Table->Entries = NULL;

    return (Table);
}

static void DestroyTable(AxHashTable *Table)
{
    // Free allocated keys
    for (size_t i = 0; i < Table->Capacity; i++)
    {
        if (Table->Entries[i].Key != NULL) {
            free((void *)Table->Entries[i].Key);
        }
    }

    // Free the array
    free(Table->Entries);

    // Reinitialize table
    CreateTable(Table);
}

static bool HashTableExpand(AxHashTable *Table, size_t Capacity)
{
    // Allocate new array
    HashEntry* Entries = calloc(Capacity, sizeof(HashEntry));

    // Initialize keys to NULL and values to unused
    for (size_t i = 0; i < Table->Capacity; i++)
    {
        Entries[i].Key = NULL;
        Entries[i].Value = (void *)AXON_HASH_UNUSED;
    }

    // Iterate through entities, moving all non-empty ones to new table
    Table->Size = 0;
    for (size_t i = 0; i < Table->Capacity; i++)
    {
        // Skip over empty entries
        HashEntry *Entry = &Table->Entries[i];
        if (Entry->Key == NULL) {
            continue;
        }

        // Insert old entry into new array
        HashEntry *NewEntry = FindEntry(Entries, Capacity, Entry->Key);
        NewEntry->Key = strdup(Entry->Key);
        NewEntry->Value = Entry->Value;
        Table->Size++;
    }

    // Free old array and update the table
    free(Table->Entries);
    Table->Entries = Entries;
    Table->Capacity = Capacity;

    return (true);
}

static bool Set(AxHashTable *Table, const char *Key, const void *Value)
{
    AXON_ASSERT(Table);

    // If we don't have enough capacity to insert a new entry, expand
    if (Table->Size + 1 > Table->Capacity * MAX_LOAD)
    {
        if (!HashTableExpand(Table, GrowCapacity(Table->Capacity))) {
            return (false);
        }
    }

    // Find entry
    HashEntry *Entry = FindEntry(Table->Entries, Table->Capacity, Key);

    // If it's a new key and has a tomestone, increment table length
    bool IsNewKey = Entry->Key == NULL;
    if (IsNewKey && Entry->Value != (void *)AXON_HASH_TOMBSTONE) {
        Table->Size++;
    }

    // Set entry
    Entry->Key = strdup(Key);
    Entry->Value = Value;

    return (true);
}

static bool Remove(AxHashTable *Table, const char *Key)
{
    AXON_ASSERT(Table);

    // Early out if table has no entries
    if (Table->Size == 0) {
        return (false);
    }

    // Find the entry
    HashEntry *Entry = FindEntry(Table->Entries, Table->Capacity, Key);
    if (Entry->Key == NULL) {
        return (false);
    }

    // Place a tombstone in the entry
    Entry->Key = NULL;
    Entry->Value = (void *)AXON_HASH_TOMBSTONE;

    return (true);
}

static void *Find(const AxHashTable *Table, const char *Key)
{
    AXON_ASSERT(Table);

    if (Table->Size == 0) {
        return (NULL);
    }

    HashEntry *Entry = FindEntry(Table->Entries, Table->Capacity, Key);
    if (Entry->Key == NULL) {
        return (NULL);
    }

    return (Entry->Value);
}

static size_t Size(const AxHashTable *Table)
{
    AXON_ASSERT(Table);
    return (Table->Size);
}

static size_t Capacity(const AxHashTable *Table)
{
    AXON_ASSERT(Table);
    return (Table->Capacity);
}

static const char *GetKeyAtIndex(AxHashTable *Table, size_t Index)
{
    AXON_ASSERT(Table);
    return (GetEntryAtIndex(Table, Index)->Key);
}

static void *GetValueAtIndex(AxHashTable *Table, size_t Index)
{
    AXON_ASSERT(Table);
    return (GetEntryAtIndex(Table, Index)->Value);
}

struct AxHashTableAPI *HashTableAPI = &(struct AxHashTableAPI) {
    .CreateTable = CreateTable,
    .DestroyTable = DestroyTable,
    .Set = Set,
    .Remove = Remove,
    .Find = Find,
    .Size = Size,
    .Capacity = Capacity,
    .GetKeyAtIndex = GetKeyAtIndex,
    .GetValueAtIndex = GetValueAtIndex
};