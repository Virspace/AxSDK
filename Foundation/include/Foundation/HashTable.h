#pragma once

#include "Foundation/Types.h"

typedef struct HashTable HashTable;

HashTable *CreateTable(size_t Capacity);
void DestroyTable(HashTable* Table);
const char *HashInsert(HashTable *Table, const char *Key, void *Value);
void *HashTableSearch(HashTable *Table, const char *Key);
size_t GetHashTableLength(HashTable *Table);
void *GetHashTableEntry(HashTable *Table, int32_t Index);
//void HashDelete(HashTable *Table, const char *Key);