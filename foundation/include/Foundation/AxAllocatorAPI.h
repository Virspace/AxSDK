#pragma once

/**
 * AxAllocatorAPI.h - Unified Allocator Factory API
 *
 * This API provides factory functions for creating different allocator types
 * (Heap, Linear, Stack) that all implement the common AxAllocator interface.
 * It also provides registry functionality to enumerate and query allocators.
 *
 * This replaces the separate AxHeapAPI, AxLinearAllocatorAPI, AxStackAllocatorAPI,
 * and AxAllocatorRegistryAPI with a single unified API.
 *
 * Usage:
 *   struct AxAllocatorAPI* api = Registry->Get(AXON_ALLOCATOR_API_NAME);
 *   struct AxAllocator* heap = api->CreateHeap("GameHeap", 1024, 4096);
 *   struct AxAllocator* linear = api->CreateLinear("FrameArena", 1024*1024);
 *   struct AxAllocator* stack = api->CreateStack("TempStack", 64*1024);
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "AxAllocator.h"

#define AXON_ALLOCATOR_API_NAME "AxonAllocatorAPI"

struct AxAllocatorAPI
{
    //=========================================================================
    // Allocator Creation
    //=========================================================================

    /**
     * Creates a heap allocator - general purpose, variable-size allocations.
     * Supports individual Free() calls for each allocation.
     *
     * @param Name Human-readable name for debugging/profiling.
     * @param InitialSize Initial committed memory size in bytes.
     * @param MaxSize Maximum reserved memory size in bytes (0 = growable).
     * @return Pointer to allocator interface, or NULL on failure.
     */
    struct AxAllocator* (*CreateHeap)(const char* Name, size_t InitialSize, size_t MaxSize);

    /**
     * Creates a linear allocator - fast bump-pointer allocation.
     * Free() is a no-op; use Reset() to free all allocations at once.
     * Ideal for per-frame allocations or level-specific resources.
     *
     * @param Name Human-readable name for debugging/profiling.
     * @param Capacity Maximum capacity in bytes.
     * @return Pointer to allocator interface, or NULL on failure.
     */
    struct AxAllocator* (*CreateLinear)(const char* Name, size_t Capacity);

    /**
     * Creates a stack allocator - LIFO allocation with markers.
     * Free() pops the most recent allocation; use markers for bulk free.
     * Useful for temporary allocations that follow a stack pattern.
     *
     * @param Name Human-readable name for debugging/profiling.
     * @param Capacity Maximum capacity in bytes.
     * @return Pointer to allocator interface, or NULL on failure.
     */
    struct AxAllocator* (*CreateStack)(const char* Name, size_t Capacity);

    //=========================================================================
    // Registry Functions
    //=========================================================================

    /**
     * Gets the total number of active allocators.
     * @return Number of registered allocators.
     */
    size_t (*GetCount)(void);

    /**
     * Gets an allocator by its index in the registry.
     * @param Index Zero-based index (0 to GetCount()-1).
     * @return Pointer to allocator, or NULL if index out of range.
     */
    struct AxAllocator* (*GetByIndex)(size_t Index);

    /**
     * Gets an allocator by its name.
     * @param Name The allocator name to search for.
     * @return Pointer to allocator, or NULL if not found.
     */
    struct AxAllocator* (*GetByName)(const char* Name);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxAllocatorAPI* AllocatorAPI;
#endif

#ifdef __cplusplus
}
#endif
