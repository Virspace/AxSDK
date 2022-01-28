#pragma once

#include "Foundation/Types.h"

typedef struct AxHashTable AxHashTable;

AxHashTable *CreateTable(size_t Capacity);
void DestroyTable(AxHashTable* Table);
const char *HashInsert(AxHashTable *Table, const char *Key, void *Value);
void *HashTableSearch(AxHashTable *Table, const char *Key);
size_t GetHashTableLength(AxHashTable *Table);
const char *GetHashTableKey(AxHashTable *Table, size_t Index);
void *GetHashTableValue(AxHashTable *Table, size_t Index);
//void HashDelete(AxHashTable *Table, const char *Key);