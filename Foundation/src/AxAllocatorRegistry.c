#include "AxAllocatorRegistry.h"
#include "AxAllocatorData.h"
#include "AxHashTable.h"

static struct AxHashTable *AllocatorInfoTable;

static void Register(struct AxAllocatorData *Data)
{
    if (!AllocatorInfoTable) {
        AllocatorInfoTable = HashTableAPI->CreateTable();
    }

    HashTableAPI->Set(AllocatorInfoTable, Data->Name, Data);
}

static size_t Length(void)
{
    return ((AllocatorInfoTable) ? HashTableAPI->Size(AllocatorInfoTable) : 0);
}

static struct AxAllocatorData *GetAllocatorDataByName(const char *Name)
{
    return ((AllocatorInfoTable) ? HashTableAPI->Find(AllocatorInfoTable, Name) : NULL);
}

static struct AxAllocatorData *GetAllocatorDataByIndex(size_t Index)
{
    return ((AllocatorInfoTable) ? HashTableAPI->GetValueAtIndex(AllocatorInfoTable, Index) : NULL);
}

struct AxAllocatorRegistryAPI *AllocatorRegistryAPI = &(struct AxAllocatorRegistryAPI) {
    .Register = Register,
    .Length = Length,
    .GetAllocatorDataByName = GetAllocatorDataByName,
    .GetAllocatorDataByIndex = GetAllocatorDataByIndex,
};