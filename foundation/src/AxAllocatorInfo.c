#include "AxAllocatorData.h"
#include "AxAllocationData.h"
#include "AxAllocatorInfo.h"
#include "AxAllocatorRegistry.h"
#include "AxArray.h"

static const char *Name(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->Name : "NULL");
}

static void *BaseAddress(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->BaseAddress : NULL);
}

static uint32_t PageSize(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->PageSize : 0);
}

static uint32_t AllocationGranularity(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->AllocationGranularity : 0);
}

static size_t BytesReserved(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->BytesReserved : 0);
}

static size_t BytesCommitted(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->BytesCommitted : 0);
}

static size_t BytesAllocated(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->BytesAllocated : 0);
}

static size_t PagesReserved(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->PagesReserved : 0);
}

static size_t PagesCommitted(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->PagesCommitted : 0);
}

static size_t NumAllocs(struct AxAllocatorData *Data)
{
    Assert(Data && "Data is NULL");
    return ((Data) ? Data->NumAllocs : 0);
}

static bool GetAllocationDataByIndex(struct AxAllocatorData *Data, size_t Index, struct AxAllocationData *AllocData)
{
    Assert(Data && "Data is NULL");
    if (Index > ArraySize(Data->AllocationData)) {
        return false;
    }

    // NOTE(mdeforge): We don't want to return a pointer to the actual allocation data
    //                 since we don't want anyone messing with it. So, we copy values instead.
    *AllocData = (struct AxAllocationData) {
        .Address = Data->AllocationData[Index].Address,
        .Size = Data->AllocationData[Index].Size
    };
}

struct AxAllocatorDataAPI *AllocatorDataAPI = &(struct AxAllocatorDataAPI) {
    .Name = Name,
    .BaseAddress = BaseAddress,
    .PageSize = PageSize,
    .AllocationGranularity = AllocationGranularity,
    .BytesReserved = BytesReserved,
    .BytesCommitted = BytesCommitted,
    .BytesAllocated = BytesAllocated,
    .PagesReserved = PagesReserved,
    .PagesCommitted = PagesCommitted,
    .NumAllocs = NumAllocs,
    .GetAllocationDataByIndex = GetAllocationDataByIndex
};