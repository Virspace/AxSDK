#include "AxAllocUtils.h"
#include "AxMath.h"

uintptr_t RoundAddressUpToPowerOfTwo(uintptr_t Address, size_t Multiple)
{
    if (!IsPowerOfTwo(Multiple)) {
        return (0);
    }

    return ((Address + Multiple - 1) & ~(Multiple - 1));
}

uintptr_t RoundAddressDownToPowerOfTwo(uintptr_t Address, size_t Multiple)
{
    if (!IsPowerOfTwo(Multiple)) {
        return (0);
    }

    return (Address & ~(Multiple - 1));
}

bool IsAligned(uintptr_t Address)
{
    // NOTE(mdeforge): The cast to uintptr_t is needed because the standard only
    // guarantees an invertible conversion to uintptr_t for void *.
    return ((Address & (sizeof(void *) - 1)) == 0);
}

size_t AlignOffset(uintptr_t Address)
{
    uintptr_t Aligned = RoundAddressUpToPowerOfTwo(Address, sizeof(void *));
    size_t Offset = AddressDistance(Address, Aligned);

    return (Offset);
}

bool IsStraddlingBoundary(uintptr_t Address, size_t AllocationSize, size_t PageSize)
{
    uintptr_t NextPageBoundary = RoundUpToPowerOfTwo(Address, PageSize);
    if (Address + AllocationSize > NextPageBoundary) {
        return true;
    }

    return false;
}

size_t AddressDistance(uintptr_t A, uintptr_t B)
{
    return (B - A);
}

uintptr_t AlignAddress(uintptr_t Address)
{
    // NOTE(mdeforge): The cast to uintptr_t is needed because the standard only
    // guarantees an invertible conversion to uintptr_t for void *.
    return (RoundAddressUpToPowerOfTwo(Address, sizeof(void *)));
}