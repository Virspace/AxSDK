#include "AxAllocatorRegistry.h"
#include "AxAllocatorInfo.h"
#include "AxHashTable.h"

static struct AxHashTable *AllocatorInfoTable;

static void Register(struct AxAllocatorInfo *Info)
{
    if (!AllocatorInfoTable) {
        AllocatorInfoTable = HashTableAPI->CreateTable();
    }

    HashTableAPI->Set(AllocatorInfoTable, Info->Name, Info);
}

static size_t Length(void)
{
    return ((AllocatorInfoTable) ? HashTableAPI->Size(AllocatorInfoTable) : 0);
}

static struct AxAllocatorInfo *GetAllocatorInfoByName(const char *Name)
{
    return ((AllocatorInfoTable) ? HashTableAPI->Find(AllocatorInfoTable, Name) : NULL);
}

static struct AxAllocatorInfo *GetAllocatorInfoByIndex(size_t Index)
{
    return ((AllocatorInfoTable) ? HashTableAPI->GetValueAtIndex(AllocatorInfoTable, Index) : NULL);
}

static const char *Name(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->Name : "NULL");
}

static void *BaseAddress(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BaseAddress : NULL);
}

static uint32_t PageSize(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->PageSize : 0);
}

static uint32_t AllocationGranularity(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->AllocationGranularity : 0);
}

static size_t BytesReserved(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BytesReserved : 0);
}

static size_t BytesCommitted(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BytesCommitted : 0);
}

static size_t BytesAllocated(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BytesAllocated : 0);
}

static size_t PagesReserved(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->PagesReserved : 0);
}

static size_t PagesCommitted(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->PagesCommitted : 0);
}

static size_t NumAllocs(struct AxAllocatorInfo *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->NumAllocs : 0);
}

struct AxAllocatorRegistryAPI *AllocatorRegistryAPI = &(struct AxAllocatorRegistryAPI) {
    .Register = Register,
    .Length = Length,
    .GetAllocatorInfoByName = GetAllocatorInfoByName,
    .GetAllocatorInfoByIndex = GetAllocatorInfoByIndex,
    .Name = Name,
    .BaseAddress = BaseAddress,
    .PageSize = PageSize,
    .BytesReserved = BytesReserved,
    .BytesCommitted = BytesCommitted,
    .BytesAllocated = BytesAllocated,
    .PagesReserved = PagesReserved,
    .PagesCommitted = PagesCommitted,
    .NumAllocs = NumAllocs
};