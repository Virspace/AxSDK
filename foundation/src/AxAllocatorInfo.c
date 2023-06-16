#include "AxAllocatorData.h"
#include "AxAllocatorRegistry.h"

static const char *Name(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->Name : "NULL");
}

static void *BaseAddress(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BaseAddress : NULL);
}

static uint32_t PageSize(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->PageSize : 0);
}

static uint32_t AllocationGranularity(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->AllocationGranularity : 0);
}

static size_t BytesReserved(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BytesReserved : 0);
}

static size_t BytesCommitted(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BytesCommitted : 0);
}

static size_t BytesAllocated(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->BytesAllocated : 0);
}

static size_t PagesReserved(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->PagesReserved : 0);
}

static size_t PagesCommitted(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->PagesCommitted : 0);
}

static size_t NumAllocs(struct AxAllocatorData *Info)
{
    Assert(Info && "Info is NULL");
    return ((Info) ? Info->NumAllocs : 0);
}

struct AxAllocatorDataAPI *AllocatorInfoAPI = &(struct AxAllocatorDataAPI) {
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