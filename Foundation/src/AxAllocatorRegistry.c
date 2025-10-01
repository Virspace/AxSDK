#include "AxAllocatorRegistry.h"
#include "AxAllocatorData.h"
#include "AxHashTable.h"

static struct AxHashTable *AllocatorInfoTable;

static void Register(struct AxAllocatorData *Data)
{
    AXON_ASSERT(Data && "AxAllocatorRegistryAPI->Register passed NULL data!");
    AXON_ASSERT(Data->Name && "AxAllocatorRegistryAPI->Register data has NULL name!");

    if (!AllocatorInfoTable) {
        AllocatorInfoTable = HashTableAPI->CreateTable();
    }

    HashTableAPI->Set(AllocatorInfoTable, Data->Name, Data);
}

static void Unregister(struct AxAllocatorData *Data)
{
    AXON_ASSERT(Data && "AxAllocatorRegistryAPI->Register passed NULL data!");
    AXON_ASSERT(Data->Name && "AxAllocatorRegistryAPI->Register data has NULL name!");

    if (AllocatorInfoTable && Data) {
        HashTableAPI->Remove(AllocatorInfoTable, Data->Name);
    }
}

static size_t Length(void)
{
    return ((AllocatorInfoTable) ? HashTableAPI->Size(AllocatorInfoTable) : 0);
}

static struct AxAllocatorData *GetAllocatorDataByName(const char *Name)
{
    AXON_ASSERT(Name && "AxAllocatorRegistryAPI->GetAllocatorDataByName passed NULL name!");
    return ((AllocatorInfoTable) ? HashTableAPI->Find(AllocatorInfoTable, Name) : NULL);
}

static struct AxAllocatorData *GetAllocatorDataByIndex(size_t Index)
{
    return ((AllocatorInfoTable) ? HashTableAPI->GetValueAtIndex(AllocatorInfoTable, Index) : NULL);
}

struct AxAllocatorRegistryAPI *AllocatorRegistryAPI = &(struct AxAllocatorRegistryAPI) {
    .Register = Register,
    .Unregister = Unregister,
    .Length = Length,
    .GetAllocatorDataByName = GetAllocatorDataByName,
    .GetAllocatorDataByIndex = GetAllocatorDataByIndex,
};