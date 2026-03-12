#pragma once

/**
 * AxAllocator.h - Unified Allocator Interface
 *
 * This is the base allocator interface that all allocator types implement.
 * Inspired by the Bitsquid/Stingray engine pattern, this enables subsystems
 * to accept any allocator type polymorphically without knowing its concrete type.
 *
 * Usage:
 *   struct AxAllocator* alloc = AllocatorAPI->CreateHeap("MyHeap", 1024, 4096);
 *   void* ptr = AxAlloc(alloc, 256);
 *   AxFree(alloc, ptr);
 *   alloc->Destroy(alloc);
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

// Forward declaration
struct AxAllocator;

/**
 * Base allocator interface - all allocators implement this.
 * The struct is designed to be embedded as the first member of concrete
 * allocator structs, allowing safe casting between base and derived types.
 */
struct AxAllocator
{
    //=========================================================================
    // Core Operations (all allocators must implement)
    //=========================================================================

    /**
     * Allocates a block of memory with the specified size and alignment.
     * @param Self Pointer to the allocator instance.
     * @param Size The number of bytes to allocate.
     * @param Alignment The required alignment (must be power of 2).
     * @return Pointer to allocated memory, or NULL on failure.
     */
    void* (*Alloc)(struct AxAllocator* Self, size_t Size, size_t Alignment);

    /**
     * Reallocates a previously allocated block to a new size.
     * @param Self Pointer to the allocator instance.
     * @param Ptr Pointer to the previously allocated block (NULL for new allocation).
     * @param OldSize The size of the existing allocation.
     * @param NewSize The desired new size.
     * @return Pointer to reallocated memory, or NULL on failure.
     */
    void* (*Realloc)(struct AxAllocator* Self, void* Ptr, size_t OldSize, size_t NewSize);

    /**
     * Frees a previously allocated block of memory.
     * Note: For linear allocators, this is a no-op. Use Reset() instead.
     * @param Self Pointer to the allocator instance.
     * @param Ptr Pointer to the block to free.
     */
    void (*Free)(struct AxAllocator* Self, void* Ptr);

    /**
     * Destroys the allocator and releases all resources.
     * After calling this, the allocator pointer is invalid.
     * @param Self Pointer to the allocator instance.
     */
    void (*Destroy)(struct AxAllocator* Self);

    //=========================================================================
    // Metadata (embedded directly in allocator)
    //=========================================================================

    const char* Name;           // Allocator name for debugging/profiling
    size_t BytesAllocated;      // Total bytes currently allocated
    size_t BytesReserved;       // Total bytes reserved (capacity)
    size_t AllocationCount;     // Number of active allocations

    //=========================================================================
    // Type-Specific Operations (NULL if not supported)
    //=========================================================================

    /**
     * Resets the allocator, freeing all allocations at once.
     * Supported by: Linear, Stack allocators.
     * @param Self Pointer to the allocator instance.
     */
    void (*Reset)(struct AxAllocator* Self);

    /**
     * Gets a marker representing the current allocation state.
     * Supported by: Stack allocator only.
     * @param Self Pointer to the allocator instance.
     * @return Opaque marker value (cast to void*).
     */
    void* (*GetMarker)(struct AxAllocator* Self);

    /**
     * Frees all allocations made after the given marker.
     * Supported by: Stack allocator only.
     * @param Self Pointer to the allocator instance.
     * @param Marker A marker previously obtained from GetMarker().
     */
    void (*FreeToMarker)(struct AxAllocator* Self, void* Marker);
};

//=============================================================================
// Convenience Macros
//=============================================================================

/** Allocate with default alignment (16 bytes) */
#define AxAlloc(alloc, size) \
    ((alloc)->Alloc((alloc), (size), AX_DEFAULT_ALIGNMENT))

/** Allocate with specified alignment */
#define AxAllocAligned(alloc, size, align) \
    ((alloc)->Alloc((alloc), (size), (align)))

/** Reallocate a block */
#define AxRealloc(alloc, ptr, oldSz, newSz) \
    ((alloc)->Realloc((alloc), (ptr), (oldSz), (newSz)))

/** Free a block */
#define AxFree(alloc, ptr) \
    ((alloc)->Free((alloc), (ptr)))

/** Default alignment for allocations */
#define AX_DEFAULT_ALIGNMENT 16

#ifdef __cplusplus
}
#endif
