#pragma once

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

// http://www.isthe.com/chongo/tech/comp/fnv/

#define FNV1_64_INIT ((uint64_t)0xcbf29ce484222325ULL)
#define FNV_64_PRIME ((uint64_t)0x100000001b3ULL)

/**
 * Perform a 64 bit Fowler/Noll/Vo FNV-1a hash on a buffer
 *
 * Recommended to use FNV1A_64_INIT as HashVal arg on the first call.
 *
 * @param String Start of a buffer to hash.
 * @param HashVal Previous hash value or 0 if first call.
 */

extern uint64_t HashStringFNV1a(const char *String, uint64_t HashVal);

/**
 * Perform a 64 bit Fowler/Noll/Vo FNV-1a hash on a buffer
 *
 * Recommended to use FNV1A_64_INIT as HashVal arg on the first call.
 *
 * @param Buffer Start of a buffer to hash.
 * @param Length of buffer in octets.
 * @param HashVal Previous hash value or 0 if first call.
 * @return 64 bit hash as a static hash type.
 */
extern uint64_t HashBufferFNV1a(void *Buffer, size_t Length, uint64_t HashVal);

#ifdef __cplusplus
}
#endif