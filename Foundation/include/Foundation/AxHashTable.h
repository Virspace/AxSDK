#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AXON_HASH_TABLE_API_NAME "AxonHashTableAPI"

typedef struct AxHashTable AxHashTable;

struct AxHashTableAPI
{
    /**
     * @param
     * @return
     */
    AxHashTable *(*CreateTable)(size_t Capacity);

    /**
     * @param
     * @return
     */
    void (*DestroyTable)(AxHashTable* Table);

    /**
     * @param
     * @return
     */
    size_t (*Length)(AxHashTable *Table);

    /**
     * @param
     * @return
     */
    const char *(*Insert)(AxHashTable *Table, const char *Key, void *Value);

    /**
     * Searches for the key in the hash table.
     * @return NULL if it doesn't exist.
     */
    void *(*Search)(AxHashTable *Table, const char *Key);

    /**
     * @param
     * @return
     */
    const char *(*GetHashTableKey)(AxHashTable *Table, size_t Index);

    /**
     * @param
     * @return
     */
    void *(*GetHashTableValue)(AxHashTable *Table, size_t Index);

    /**
     * @param
     * @return
     */
    //void (*HashDelete)(AxHashTable *Table, const char *Key);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxHashTableAPI *HashTableAPI;
#endif

#ifdef __cplusplus
}
#endif