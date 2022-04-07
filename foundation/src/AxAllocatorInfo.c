#include "AxAllocatorInfo.h"
#include "AxArray.h"

static struct AxAllocatorInfo **AllocatorInfoArray;

static void Register(struct AxAllocatorInfo *Info)
{
    ArrayPush(AllocatorInfoArray, Info);
}

static size_t Size(void)
{
    return (ArraySize(AllocatorInfoArray));
}

static struct AxAllocatorInfo GetInfoByIndex(size_t Index)
{
    return (*AllocatorInfoArray[Index]);
}

struct AxAllocatorInfoRegistryAPI *AllocatorInfoRegistryAPI = &(struct AxAllocatorInfoRegistryAPI) {
    .Register = Register,
    .Size = Size,
    .GetInfoByIndex = GetInfoByIndex,
};