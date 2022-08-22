#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_HASH_TABLE_API_NAME "AxonHashTableAPI"

/*
    This hash table implementation uses open addressing, linear probing, and the
    FNV-1 hash function. The capacity is always a power of two and automatically
    expands and re-hashes when 75% full. This gives it an average fill rate of
    (75 + 75 / 2) / 2 = 56%. Unused slots are marked with an unused sentinel value
    and removals are marked with a tombstone sentinel value.

    CAUTION!
    Do not store pointers to values! If the table expands, you may lose them!

    Generally speaking, open addressing is better for small record sizes. They
    are more space efficient than separate chaining since they don't need to
    store pointers or allocate space outside the table, which has the added
    benefit of making serialization easier. Because open addressing uses
    internal storage, insertions avoid the time overhead of memory allocation
    and avoid the extra indirection required for chaining external storage.
    With small record sizes, this data locality can yield better performance
    than chained tables, particularly for lookups.

    This hash table is a poor choice for large elements since those elements
    fill the entire cache line (thus voiding the cache advantage) and a large
    amount of space is wasted on large, empty table slots.

    -------------------------------
    When to Use This Hash Table
    -------------------------------
    * You have small record sizes that can fit within a cache line

    -------------------------------
    When to Not Use This Hash Table
    -------------------------------
    * You have high load factors
    * You have large record sizes (larger than a cache line)
    * You have data that varies greatly in size
*/

typedef struct AxHashTable AxHashTable;
struct AxHashTableAPI
{
    /**
     * Creates an empty hash table.
     * @return A pointer to the newly constructed hash table.
     */
    AxHashTable *(*CreateTable)(void);

    /**
     * Destroys the table, frees allocated keys and the table itself.
     * @param Table The target table.
     */
    void (*DestroyTable)(AxHashTable* Table);

    /**
     * Inserts a key and value
     * @param Table The target table.
     * @param Key The entry key.
     * @param Value the entry value.
     * @return The key if successful, otherwise NULL.
     */
    bool (*Set)(AxHashTable *Table, const char *Key, void *Value);

    /**
     * @param
     * @return
     */
    bool (*Remove)(AxHashTable *Table, const char *Key);

    /**
     * Searches for the key in the hash table.
     * @param Table The target table.
     * @param Key The entry key.
     * @return NULL if it doesn't exist.
     */
    void *(*Find)(const AxHashTable *Table, const char *Key);

    /**
     * Gets the current number of entries that are in the hash table.
     * @param Table The target table.
     * @return The number of entries in the hash table
     */
    size_t (*Size)(const AxHashTable *Table);

    /**
     * Gets the max number of entries that the table can contain at its present capacity.
     * @param Table The target table.
     * @return The max number of entries the table contains at its present capacity.
     */
    size_t (*Capacity)(const AxHashTable *Table);

    /**
     * @param
     * @return
     */
    const char *(*GetKeyAtIndex)(AxHashTable *Table, size_t Index);

    /**
     * @param
     * @return
     */
    void *(*GetValueAtIndex)(AxHashTable *Table, size_t Index);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxHashTableAPI *HashTableAPI;
#endif

#ifdef __cplusplus
}
#endif