#include "AxResource/AxShaderManager.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxTypes.h"
#include "AxOpenGL/AxOpenGL.h"
#include "Foundation/AxPlatform.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/**
 * Internal shader slot structure
 */
typedef struct {
    AxShaderData* Data;      // Shader data (NULL if slot is free)
    uint32_t RefCount;       // Reference count (0 = unused)
    uint32_t Generation;     // Generation counter for handle validation
    char VertexPath[256];    // Cached for debugging/reloading
    char FragmentPath[256];  // Cached for debugging/reloading
} ShaderSlot;

/**
 * Shader Manager State
 */
static struct {
    ShaderSlot* Slots;           // Dynamic array of shader slots
    uint32_t Capacity;           // Total number of slots allocated
    uint32_t Count;              // Number of slots in use
    uint32_t NextFreeIndex;      // Hint for next free slot search
    struct AxOpenGLAPI* RenderAPI; // OpenGL API for shader operations
    struct AxPlatformAPI* PlatformAPI; // PlatformAPI for platform operations
    bool Initialized;            // Initialization flag
} State = {0};

///////////////////////////////////////////////////////////////
// Internal Helper Functions
///////////////////////////////////////////////////////////////

static uint32_t FindFreeSlot(void)
{
    // Start search from hint
    for (uint32_t i = State.NextFreeIndex; i < State.Capacity; i++) {
        if (State.Slots[i].RefCount == 0) {
            State.NextFreeIndex = i + 1;
            return (i);
        }
    }

    // Wrap around and search from beginning
    for (uint32_t i = 1; i < State.NextFreeIndex; i++) {
        if (State.Slots[i].RefCount == 0) {
            State.NextFreeIndex = i + 1;
            return (i);
        }
    }

    // No free slots, need to grow
    return (0);
}

static bool GrowCapacity(void)
{
    uint32_t NewCapacity = State.Capacity * 2;
    if (NewCapacity == 0) {
        NewCapacity = 16; // Minimum initial capacity
    }

    ShaderSlot* NewSlots = (ShaderSlot*)realloc(State.Slots, NewCapacity * sizeof(ShaderSlot));
    if (!NewSlots) {
        fprintf(stderr, "ShaderManager: Failed to grow capacity to %u\n", NewCapacity);
        return (false);
    }

    // Zero-initialize new slots
    memset(NewSlots + State.Capacity, 0, (NewCapacity - State.Capacity) * sizeof(ShaderSlot));

    State.Slots = NewSlots;
    State.Capacity = NewCapacity;

    return (true);
}

static bool IsHandleValid(AxShaderHandle Handle)
{
    if (Handle.Index == 0 || Handle.Index >= State.Capacity) {
        return (false);
    }

    ShaderSlot* Slot = &State.Slots[Handle.Index];
    if (Slot->Generation != Handle.Generation || Slot->RefCount == 0) {
        return (false);
    }

    return (true);
}

///////////////////////////////////////////////////////////////
// API Implementation
///////////////////////////////////////////////////////////////

static void Initialize(struct AxAPIRegistry* Registry, uint32_t InitialCapacity)
{
    if (State.Initialized) {
        fprintf(stderr, "ShaderManager: Already initialized\n");
        return;
    }

    // Get OpenGL API for shader operations
    State.RenderAPI = (struct AxOpenGLAPI*)Registry->Get(AXON_OPENGL_API_NAME);
    if (!State.RenderAPI) {
        fprintf(stderr, "ShaderManager: Failed to get OpenGL API\n");
        return;
    }

    // Get Platform API for platform operations
    State.PlatformAPI = (struct AxPlatformAPI*)Registry->Get(AXON_PLATFORM_API_NAME);
    if (!State.PlatformAPI) {
        fprintf(stderr, "ShaderManager: Failed to get Platform API\n");
        return;
    }

    // Allocate initial capacity
    if (InitialCapacity == 0) {
        InitialCapacity = 32; // Default capacity
    }

    State.Slots = (ShaderSlot*)calloc(InitialCapacity, sizeof(ShaderSlot));
    if (!State.Slots) {
        fprintf(stderr, "ShaderManager: Failed to allocate initial capacity\n");
        return;
    }

    State.Capacity = InitialCapacity;
    State.Count = 0;
    State.NextFreeIndex = 1; // Index 0 is reserved for invalid handle
    State.Initialized = true;

    printf("ShaderManager: Initialized with capacity %u\n", InitialCapacity);
}

static void Shutdown(void)
{
    if (!State.Initialized) {
        return;
    }

    // Clean up all active shaders
    for (uint32_t i = 1; i < State.Capacity; i++) {
        ShaderSlot* Slot = &State.Slots[i];
        if (Slot->RefCount > 0 && Slot->Data) {
            State.RenderAPI->DestroyProgram(Slot->Data->ShaderHandle);
            free(Slot->Data);
            Slot->Data = NULL;
        }
    }

    free(State.Slots);
    memset(&State, 0, sizeof(State));

    printf("ShaderManager: Shutdown complete\n");
}

static AxShaderHandle CreateShader(const char* VertexShaderPath, const char* FragmentShaderPath)
{
    if (!State.Initialized) {
        fprintf(stderr, "ShaderManager: Not initialized\n");
        return (AX_INVALID_SHADER_HANDLE);
    }

    if (!VertexShaderPath || !FragmentShaderPath) {
        fprintf(stderr, "ShaderManager: NULL shader paths\n");
        return (AX_INVALID_SHADER_HANDLE);
    }

    // Find or allocate a free slot
    uint32_t Index = FindFreeSlot();
    if (Index == 0) {
        if (!GrowCapacity()) {
            return (AX_INVALID_SHADER_HANDLE);
        }
        Index = FindFreeSlot();
    }

    ShaderSlot* Slot = &State.Slots[Index];

    // Load shaders
    struct AxPlatformFileAPI *FileAPI = State.PlatformAPI->FileAPI;
    AxFile VertexShader = FileAPI->OpenForRead(VertexShaderPath);
    AXON_ASSERT(FileAPI->IsValid(VertexShader) && "Vertex shader file size is invalid!");
    AxFile FragmentShader = FileAPI->OpenForRead(FragmentShaderPath);
    AXON_ASSERT(FileAPI->IsValid(FragmentShader) && "Fragment shader file size is invalid!");

    uint64_t VertexShaderFileSize = FileAPI->Size(VertexShader);
    uint64_t FragmentShaderFileSize = FileAPI->Size(FragmentShader);

    // Allocate text buffers
    char *VertexShaderBuffer = (char *)malloc(VertexShaderFileSize + 1);
    char *FragmentShaderBuffer = (char *)malloc(FragmentShaderFileSize + 1);

    // Read shader text
    FileAPI->Read(VertexShader, VertexShaderBuffer, (uint32_t)VertexShaderFileSize);
    FileAPI->Read(FragmentShader, FragmentShaderBuffer, (uint32_t)FragmentShaderFileSize);

    // Null-terminate
    VertexShaderBuffer[VertexShaderFileSize] = '\0';
    FragmentShaderBuffer[FragmentShaderFileSize] = '\0';

    // Compile shader using OpenGL API
    uint32_t ShaderProgram = State.RenderAPI->CreateProgram("", VertexShaderBuffer, FragmentShaderBuffer);
    if (ShaderProgram == 0) {
        fprintf(stderr, "ShaderManager: Failed to compile shader: %s, %s\n",
                VertexShaderPath, FragmentShaderPath);
        return (AX_INVALID_SHADER_HANDLE);
    }

    SAFE_FREE(VertexShaderBuffer);
    SAFE_FREE(FragmentShaderBuffer);

    // Allocate and populate shader data
    AxShaderData* ShaderData = (AxShaderData*)calloc(1, sizeof(AxShaderData));
    if (!ShaderData) {
        State.RenderAPI->DestroyProgram(ShaderProgram);
        return (AX_INVALID_SHADER_HANDLE);
    }

    ShaderData->ShaderHandle = ShaderProgram;
    State.RenderAPI->GetAttributeLocations(ShaderProgram, ShaderData);

    // Initialize slot
    Slot->Data = ShaderData;
    Slot->RefCount = 1; // Initial reference
    Slot->Generation++; // Increment generation for handle validation
    strncpy(Slot->VertexPath, VertexShaderPath, sizeof(Slot->VertexPath) - 1);
    strncpy(Slot->FragmentPath, FragmentShaderPath, sizeof(Slot->FragmentPath) - 1);

    State.Count++;

    AxShaderHandle Handle = {Index, Slot->Generation};
    printf("ShaderManager: Created shader [%u:%u] - %s, %s (Program=%u)\n",
           Handle.Index, Handle.Generation, VertexShaderPath, FragmentShaderPath, ShaderProgram);

    return (Handle);
}

static void AddRef(AxShaderHandle Handle)
{
    if (!IsHandleValid(Handle)) {
        fprintf(stderr, "ShaderManager: AddRef called with invalid handle [%u:%u]\n",
                Handle.Index, Handle.Generation);
        return;
    }

    State.Slots[Handle.Index].RefCount++;
}

static void Release(AxShaderHandle Handle)
{
    if (!IsHandleValid(Handle)) {
        fprintf(stderr, "ShaderManager: Release called with invalid handle [%u:%u]\n",
                Handle.Index, Handle.Generation);
        return;
    }

    ShaderSlot* Slot = &State.Slots[Handle.Index];
    Slot->RefCount--;

    printf("ShaderManager: Released shader [%u:%u], RefCount now %u\n",
           Handle.Index, Handle.Generation, Slot->RefCount);

    // Clean up when refcount reaches zero
    if (Slot->RefCount == 0) {
        if (Slot->Data) {
            State.RenderAPI->DestroyProgram(Slot->Data->ShaderHandle);
            free(Slot->Data);
            Slot->Data = NULL;
        }
        State.Count--;

        printf("ShaderManager: Destroyed shader [%u:%u] - %s, %s\n",
               Handle.Index, Handle.Generation, Slot->VertexPath, Slot->FragmentPath);

        // Update free slot hint
        if (Handle.Index < State.NextFreeIndex) {
            State.NextFreeIndex = Handle.Index;
        }
    }
}

static const AxShaderData* GetShaderData(AxShaderHandle Handle)
{
    if (!IsHandleValid(Handle)) {
        return (NULL);
    }

    return (State.Slots[Handle.Index].Data);
}

static uint32_t GetShaderProgram(AxShaderHandle Handle)
{
    if (!IsHandleValid(Handle)) {
        return (0);
    }

    ShaderSlot* Slot = &State.Slots[Handle.Index];
    return (Slot->Data ? Slot->Data->ShaderHandle : 0);
}

static uint32_t GetRefCount(AxShaderHandle Handle)
{
    if (!IsHandleValid(Handle)) {
        return (0);
    }

    return (State.Slots[Handle.Index].RefCount);
}

static bool IsValid(AxShaderHandle Handle)
{
    return (IsHandleValid(Handle));
}

static uint32_t GetActiveShaderCount(void)
{
    return (State.Count);
}

static void CollectGarbage(void)
{
    // For now, garbage collection is automatic when refcount hits zero
    // Future: Could compact slots and reduce capacity if usage is low
    printf("ShaderManager: Garbage collection - %u active shaders\n", State.Count);
}

///////////////////////////////////////////////////////////////
// API Structure
///////////////////////////////////////////////////////////////

struct AxShaderManagerAPI* ShaderManagerAPI = &(struct AxShaderManagerAPI) {
    .Initialize = Initialize,
    .Shutdown = Shutdown,
    .CreateShader = CreateShader,
    .AddRef = AddRef,
    .Release = Release,
    .GetShaderData = GetShaderData,
    .GetShaderProgram = GetShaderProgram,
    .GetRefCount = GetRefCount,
    .IsValid = IsValid,
    .GetActiveShaderCount = GetActiveShaderCount,
    .CollectGarbage = CollectGarbage
};