#include "AxAllocatorInfo.h"

//#define AXARRAY_IMPLEMENTATION
#include "AxArray.h"

static struct AxAllocatorInfo **AllocatorsArray;

static void Register(struct AxAllocatorInfo *BaseAllocator)
{
    ArrayPush(AllocatorsArray, BaseAllocator);
}

static size_t Count(void)
{
    return (ArraySize(AllocatorsArray));
}

static struct AxAllocatorInfo Stats(size_t Index)
{
    return (*AllocatorsArray[Index]);
}

struct AxAllocatorInfoRegistryAPI *AllocatorInfoRegistryAPI = &(struct AxAllocatorInfoRegistryAPI) {
    .Register = Register,
    .Count = Count,
    .Stats = Stats,
};