# Linear Allocator Usage Guide

## Overview

The AxSDK Linear Allocator provides O(1) allocation performance with bulk deallocation, making it ideal for temporary memory that follows predictable allocation patterns like scene loading.

## Key Characteristics

### Performance
- **Allocation**: O(1) constant time
- **Deallocation**: Bulk only - frees all allocations at once
- **Memory Layout**: Sequential, cache-friendly allocation pattern

### Memory Management
- **Reserve**: Virtual memory reserved at creation (not committed)
- **Commit**: Physical memory committed on-demand by page
- **Alignment**: All allocations are pointer-aligned (8-byte on x64)
- **Page Boundary**: Handles cross-page allocations automatically

## Ideal Use Cases

### Scene Loading
```c
// Create allocator for scene data
AxLinearAllocator *SceneAllocator = LinearAllocatorAPI->Create("SceneData", Megabytes(8));

// Allocate scene objects sequentially
AxSceneObject *RootObject = LinearAllocatorAPI->Alloc(SceneAllocator, sizeof(AxSceneObject), __FILE__, __LINE__);
AxSceneObject *Child1 = LinearAllocatorAPI->Alloc(SceneAllocator, sizeof(AxSceneObject), __FILE__, __LINE__);

// Allocate strings for asset paths
char *MeshPath = LinearAllocatorAPI->Alloc(SceneAllocator, 256, __FILE__, __LINE__);
char *MaterialPath = LinearAllocatorAPI->Alloc(SceneAllocator, 256, __FILE__, __LINE__);

// Free all scene data at once when done
LinearAllocatorAPI->Free(SceneAllocator, __FILE__, __LINE__);
```

### Temporary String Processing
```c
// Ideal for parsing operations where many temporary strings are needed
AxLinearAllocator *ParseAllocator = LinearAllocatorAPI->Create("Parser", Kilobytes(512));

// Allocate tokens during parsing
for (int i = 0; i < TokenCount; ++i) {
    char *Token = LinearAllocatorAPI->Alloc(ParseAllocator, TokenLengths[i], __FILE__, __LINE__);
    // Process token...
}

// Free all parsing memory at once
LinearAllocatorAPI->Free(ParseAllocator, __FILE__, __LINE__);
```

### Frame-Based Allocations
```c
// Game loop with per-frame temporary allocations
AxLinearAllocator *FrameAllocator = LinearAllocatorAPI->Create("Frame", Megabytes(2));

while (GameRunning) {
    // Allocate temporary data for this frame
    TempData *Temp = LinearAllocatorAPI->Alloc(FrameAllocator, sizeof(TempData), __FILE__, __LINE__);

    // Process frame...

    // Reset for next frame
    LinearAllocatorAPI->Free(FrameAllocator, __FILE__, __LINE__);
}
```

## Memory Sizing Guidelines

### Scene Data
- **Small scenes**: 1-4 MB (basic objects, few assets)
- **Medium scenes**: 4-16 MB (complex hierarchies, many assets)
- **Large scenes**: 16-64 MB (detailed environments, many objects)

### String/Parse Data
- **Text parsing**: 64-512 KB (typical file parsing)
- **Asset path storage**: 256 KB - 2 MB (depending on asset count)

### General Purpose
- **Start conservative**: Begin with smaller sizes and measure actual usage
- **Round to allocation granularity**: Use powers of 2 or multiples of 64KB

## Limitations and Considerations

### Cannot Free Individual Allocations
```c
// ❌ WRONG - No individual free function
// There is no LinearAllocatorAPI->FreePtr(ptr);

// ✅ CORRECT - Free all allocations at once
LinearAllocatorAPI->Free(Allocator, __FILE__, __LINE__);
```

### Memory Commitment
- Memory is committed by page (4KB on Windows)
- Large allocations commit multiple pages immediately
- Memory stays committed until Free() is called
- Use appropriate initial size to avoid virtual memory fragmentation

### Alignment Requirements
- All allocations are automatically aligned to pointer size (8 bytes on x64)
- Small allocations may have padding for alignment
- Consider this when calculating memory requirements

### Platform Differences
- **Windows**: Uses VirtualAlloc for reserve/commit pattern
- **Linux**: Uses mmap for memory management
- **Page sizes**: Typically 4KB but may vary by platform

## Best Practices

### Size Selection
```c
// Consider padding and alignment overhead
size_t EstimatedSize = (ObjectCount * sizeof(AxSceneObject)) +
                       (StringCount * AverageStringLength) +
                       (EstimatedSize * 0.1f); // 10% padding for alignment

AxLinearAllocator *Allocator = LinearAllocatorAPI->Create("Scene", EstimatedSize);
```

### Error Handling
```c
void *Memory = LinearAllocatorAPI->Alloc(Allocator, Size, __FILE__, __LINE__);
if (!Memory) {
    // Handle allocation failure - likely out of reserved space
    printf("Linear allocator out of space: requested %zu bytes\\n", Size);
    return (false);
}
```

### Monitoring Usage
```c
// Check allocator statistics
AxAllocatorData *Data = AllocatorRegistryAPI->GetAllocatorDataByName("SceneName");
printf("Committed: %zu bytes, Allocated: %zu bytes\\n",
       AllocatorDataAPI->BytesCommitted(Data),
       AllocatorDataAPI->BytesAllocated(Data));
```

### Memory Reuse
```c
// Reuse allocator for similar allocation patterns
LinearAllocatorAPI->Free(SceneAllocator, __FILE__, __LINE__); // Reset to beginning
// Now can allocate new scene data with same allocator
```

## Performance Characteristics

### Allocation Speed
- **First allocation**: May cause page commitment (slower)
- **Subsequent allocations**: Pointer arithmetic only (fastest)
- **Cross-page allocation**: May cause additional page commitment

### Memory Efficiency
- **Fragmentation**: None (sequential allocation)
- **Overhead**: Minimal (only alignment padding)
- **Locality**: Excellent (sequential memory access)

### When NOT to Use

- **Long-lived objects with different lifetimes**: Use heap allocator instead
- **Frequent individual deallocations**: Use general-purpose allocator
- **Unknown allocation patterns**: Use heap allocator for flexibility
- **Multi-threaded allocation**: Linear allocator is not thread-safe

## Integration with AxSDK

The linear allocator integrates with the AxSDK allocator registry system, enabling:
- **Monitoring**: All allocations tracked for debugging
- **Registry**: Named allocators for easy lookup
- **Statistics**: Memory usage reporting and analysis

This makes it ideal for scene loading where predictable, bulk allocation and deallocation patterns are common.