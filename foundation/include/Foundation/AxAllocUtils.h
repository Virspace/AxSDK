#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Rounds a address up to the nearest power of two multiple.
 * @param Address The address to round up.
 * @param PowerOfTwo The power of two to round up to.
 * @return The nearest power of two multiple, zero if Multiple is not a power of two.
 */
uintptr_t RoundAddressUpToPowerOfTwo(uintptr_t Address, size_t Multiple);

/**
 * Rounds a address down to the nearest power of two multiple.
 * @param Address The address to round down.
 * @param Multiple The power of two to round down to.
 * @return The nearest power of two multiple, zero if Multiple is not a power of two.
 */
uintptr_t RoundAddressDownToPowerOfTwo(uintptr_t Address, size_t Multiple);

bool IsAligned(uintptr_t Address);

size_t AlignOffset(uintptr_t Address);

bool IsStraddlingBoundary(uintptr_t Address, size_t AllocationSize, size_t PageSize);

size_t AddressDistance(uintptr_t A, uintptr_t B);

uintptr_t AlignAddress(uintptr_t Address);

#ifdef __cplusplus
}
#endif