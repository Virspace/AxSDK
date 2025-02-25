#include "AxHash.h"

uint64_t HashStringFNV1a(const char *String, uint64_t HashVal)
{
    unsigned char *s = (unsigned char *)String;	// unsigned string

    // FNV-1a hash each octet of the string
    while (*s)
    {
        // NOTE(mdeforge): Avoid sign extension by using *(unsigned char *) instead of uint64_t
        HashVal ^= *(unsigned char *)s++; // xor the bottom with the current octet
        HashVal *= FNV_64_PRIME;   // Multiply by the 64-bit FNV magic prime mod 2^64
    }

    return (HashVal);
}

uint64_t HashBufferFNV1a(void *Buffer, size_t Length, uint64_t HashVal)
{
    unsigned char *bp = (unsigned char *)Buffer; // Start of buffer
    unsigned char *be = bp + Length;             // Beyond end of buffer

    // FNV-1a hash each octet of the buffer
    while (bp < be)
    {
        HashVal ^= (unsigned char)*bp++; // xor the bottom with the current octet */
        HashVal *= FNV_64_PRIME; // Multiply by the 64 bit FNV magic prime mod 2^64
    }

    return (HashVal);
}