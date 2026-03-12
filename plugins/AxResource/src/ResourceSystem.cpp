/**
 * ResourceSystem.cpp - Internal Resource System Implementation
 *
 * Implements the core resource management functionality including:
 * - Handle-based slot management with generation tracking
 * - Reference counting for automatic lifetime management
 * - Deferred destruction queue
 * - Resource loading (textures, meshes, shaders, materials, models)
 *
 * Phase 3: Added actual resource loading implementations
 * Phase 4: Added model loading convenience API
 * Phase 6: Added handle-based model tracking with ModelManager
 */

#include "ResourceSystem.h"
#include "Foundation/AxAllocatorAPI.h"
#include "Foundation/AxPlatform.h"
#include "Foundation/AxTypes.h"
#include "AxOpenGL/AxOpenGL.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "AxLog/AxLog.h"

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <functional>

// stb_image for texture loading
// Define implementation here since AxResource is a standalone plugin
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// cgltf for GLTF loading
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

//=============================================================================
// ResourceSlotArray Implementation
//=============================================================================

template<typename T>
ResourceSlotArray<T>::ResourceSlotArray()
    : m_Slots(nullptr)
    , m_Allocator(nullptr)
    , m_Capacity(0)
    , m_UsedCount(0)
    , m_FreeListHead(0)
{
}

template<typename T>
ResourceSlotArray<T>::~ResourceSlotArray()
{
    Shutdown();
}

template<typename T>
bool ResourceSlotArray<T>::Initialize(struct AxAllocator* Allocator, uint32_t InitialCapacity)
{
    if (!Allocator || InitialCapacity == 0) {
        return false;
    }

    m_Allocator = Allocator;

    // Allocate slot array
    size_t Size = sizeof(ResourceSlot<T>) * InitialCapacity;
    m_Slots = static_cast<ResourceSlot<T>*>(AxAlloc(m_Allocator, Size));
    if (!m_Slots) {
        return false;
    }

    m_Capacity = InitialCapacity;
    m_UsedCount = 0;

    // Initialize all slots
    for (uint32_t i = 0; i < m_Capacity; ++i) {
        m_Slots[i].Data = nullptr;
        m_Slots[i].RefCount = 0;
        m_Slots[i].Generation = 1; // Start at 1 so generation 0 is always invalid
        m_Slots[i].NextFree = i + 1;
        m_Slots[i].InUse = false;
    }

    // Slot 0 is reserved as invalid
    m_Slots[0].InUse = true; // Mark as "in use" so it's never allocated
    m_Slots[0].Generation = 0; // Generation 0 ensures AX_INVALID_HANDLE never matches
    m_FreeListHead = 1; // Free list starts at slot 1

    return true;
}

template<typename T>
void ResourceSlotArray<T>::Shutdown()
{
    if (m_Slots && m_Allocator) {
        // Note: Actual resource cleanup (GPU resources, etc.) should be done
        // by the caller before calling Shutdown
        AxFree(m_Allocator, m_Slots);
        m_Slots = nullptr;
    }
    m_Allocator = nullptr;
    m_Capacity = 0;
    m_UsedCount = 0;
    m_FreeListHead = 0;
}

template<typename T>
bool ResourceSlotArray<T>::Grow(uint32_t NewCapacity)
{
    if (NewCapacity <= m_Capacity) {
        return true;
    }

    // Allocate new array
    size_t OldSize = sizeof(ResourceSlot<T>) * m_Capacity;
    size_t NewSize = sizeof(ResourceSlot<T>) * NewCapacity;
    ResourceSlot<T>* NewSlots = static_cast<ResourceSlot<T>*>(
        AxRealloc(m_Allocator, m_Slots, OldSize, NewSize));

    if (!NewSlots) {
        return false;
    }

    // Initialize new slots
    for (uint32_t i = m_Capacity; i < NewCapacity; ++i) {
        NewSlots[i].Data = nullptr;
        NewSlots[i].RefCount = 0;
        NewSlots[i].Generation = 1;
        NewSlots[i].NextFree = i + 1;
        NewSlots[i].InUse = false;
    }

    // Link new slots to free list
    if (m_FreeListHead >= m_Capacity) {
        // Free list was empty, point to first new slot
        m_FreeListHead = m_Capacity;
    } else {
        // Find end of existing free list and link to new slots
        uint32_t Last = m_FreeListHead;
        while (m_Slots[Last].NextFree < m_Capacity) {
            Last = m_Slots[Last].NextFree;
        }
        NewSlots[Last].NextFree = m_Capacity;
    }

    m_Slots = NewSlots;
    m_Capacity = NewCapacity;
    return true;
}

template<typename T>
struct AxResourceHandle ResourceSlotArray<T>::Allocate()
{
    // Check if we need to grow
    if (m_FreeListHead >= m_Capacity) {
        uint32_t NewCapacity = m_Capacity == 0 ? 64 : m_Capacity * 2;
        if (!Grow(NewCapacity)) {
            return AX_INVALID_HANDLE;
        }
    }

    // Pop from free list
    uint32_t Index = m_FreeListHead;
    m_FreeListHead = m_Slots[Index].NextFree;

    // Initialize slot
    m_Slots[Index].Data = nullptr;
    m_Slots[Index].RefCount = 1; // New resources start with refcount of 1
    m_Slots[Index].InUse = true;
    // Generation already incremented when freed, or initialized to 1

    m_UsedCount++;

    struct AxResourceHandle Handle;
    Handle.Index = Index;
    Handle.Generation = m_Slots[Index].Generation;
    return Handle;
}

template<typename T>
void ResourceSlotArray<T>::Free(uint32_t Index)
{
    if (Index == 0 || Index >= m_Capacity || !m_Slots[Index].InUse) {
        return;
    }

    // Mark as free
    m_Slots[Index].Data = nullptr;
    m_Slots[Index].RefCount = 0;
    m_Slots[Index].Generation++; // Increment generation to invalidate old handles
    m_Slots[Index].InUse = false;

    // Push to free list
    m_Slots[Index].NextFree = m_FreeListHead;
    m_FreeListHead = Index;

    m_UsedCount--;
}

template<typename T>
bool ResourceSlotArray<T>::IsValid(struct AxResourceHandle Handle) const
{
    if (Handle.Index == 0 || Handle.Index >= m_Capacity) {
        return false;
    }
    const ResourceSlot<T>& Slot = m_Slots[Handle.Index];
    return Slot.InUse && Slot.Generation == Handle.Generation;
}

template<typename T>
ResourceSlot<T>* ResourceSlotArray<T>::GetSlot(struct AxResourceHandle Handle)
{
    if (!IsValid(Handle)) {
        return nullptr;
    }
    return &m_Slots[Handle.Index];
}

template<typename T>
const ResourceSlot<T>* ResourceSlotArray<T>::GetSlot(struct AxResourceHandle Handle) const
{
    if (!IsValid(Handle)) {
        return nullptr;
    }
    return &m_Slots[Handle.Index];
}

template<typename T>
ResourceSlot<T>* ResourceSlotArray<T>::GetSlotByIndex(uint32_t Index)
{
    if (Index >= m_Capacity) {
        return nullptr;
    }
    return &m_Slots[Index];
}

template<typename T>
const ResourceSlot<T>* ResourceSlotArray<T>::GetSlotByIndex(uint32_t Index) const
{
    if (Index >= m_Capacity) {
        return nullptr;
    }
    return &m_Slots[Index];
}

// Explicit template instantiations
template class ResourceSlotArray<AxTexture>;
template class ResourceSlotArray<AxMesh>;
template class ResourceSlotArray<AxShaderData>;
template class ResourceSlotArray<AxMaterialDesc>;
template class ResourceSlotArray<AxModelData>;

//=============================================================================
// ResourceSystem Implementation
//=============================================================================

ResourceSystem::ResourceSystem()
    : m_Allocator(nullptr)
    , m_Registry(nullptr)
    , m_RenderAPI(nullptr)
    , m_PlatformAPI(nullptr)
    , m_PendingDeletions(nullptr)
    , m_PendingCount(0)
    , m_PendingCapacity(0)
    , m_TexturePathCache(nullptr)
    , m_TexturePathCacheCapacity(0)
    , m_TexturePathCacheCount(0)
    , m_Initialized(false)
{
}

ResourceSystem::~ResourceSystem()
{
    if (m_Initialized) {
        Shutdown();
    }
}

ResourceSystem& ResourceSystem::Get()
{
    static ResourceSystem s_Instance;
    return s_Instance;
}

bool ResourceSystem::Initialize(struct AxAPIRegistry* Registry,
                                 struct AxAllocator* Allocator,
                                 const struct AxResourceInitOptions* Options)
{
    if (m_Initialized) {
        return true; // Already initialized
    }

    if (!Registry || !Allocator) {
        return false;
    }

    m_Registry = Registry;
    m_Allocator = Allocator;

    // Get dependencies from registry
    m_RenderAPI = static_cast<struct AxOpenGLAPI*>(Registry->Get(AXON_OPENGL_API_NAME));
    m_PlatformAPI = static_cast<struct AxPlatformAPI*>(Registry->Get(AXON_PLATFORM_API_NAME));

    // Use default capacities if no options provided
    uint32_t TextureCapacity = Options ? Options->InitialTextureCapacity : AX_RESOURCE_DEFAULT_TEXTURE_CAPACITY;
    uint32_t MeshCapacity = Options ? Options->InitialMeshCapacity : AX_RESOURCE_DEFAULT_MESH_CAPACITY;
    uint32_t ShaderCapacity = Options ? Options->InitialShaderCapacity : AX_RESOURCE_DEFAULT_SHADER_CAPACITY;
    uint32_t MaterialCapacity = Options ? Options->InitialMaterialCapacity : AX_RESOURCE_DEFAULT_MATERIAL_CAPACITY;
    uint32_t ModelCapacity = Options ? Options->InitialModelCapacity : AX_RESOURCE_DEFAULT_MODEL_CAPACITY;

    // Ensure non-zero capacities
    if (TextureCapacity == 0) TextureCapacity = AX_RESOURCE_DEFAULT_TEXTURE_CAPACITY;
    if (MeshCapacity == 0) MeshCapacity = AX_RESOURCE_DEFAULT_MESH_CAPACITY;
    if (ShaderCapacity == 0) ShaderCapacity = AX_RESOURCE_DEFAULT_SHADER_CAPACITY;
    if (MaterialCapacity == 0) MaterialCapacity = AX_RESOURCE_DEFAULT_MATERIAL_CAPACITY;
    if (ModelCapacity == 0) ModelCapacity = AX_RESOURCE_DEFAULT_MODEL_CAPACITY;

    // Initialize slot arrays
    if (!m_Textures.Initialize(Allocator, TextureCapacity)) {
        return false;
    }
    if (!m_Meshes.Initialize(Allocator, MeshCapacity)) {
        m_Textures.Shutdown();
        return false;
    }
    if (!m_Shaders.Initialize(Allocator, ShaderCapacity)) {
        m_Textures.Shutdown();
        m_Meshes.Shutdown();
        return false;
    }
    if (!m_Materials.Initialize(Allocator, MaterialCapacity)) {
        m_Textures.Shutdown();
        m_Meshes.Shutdown();
        m_Shaders.Shutdown();
        return false;
    }
    if (!m_Models.Initialize(Allocator, ModelCapacity)) {
        m_Textures.Shutdown();
        m_Meshes.Shutdown();
        m_Shaders.Shutdown();
        m_Materials.Shutdown();
        return false;
    }

    // Initialize pending deletion queue
    m_PendingCapacity = 64;
    m_PendingDeletions = static_cast<PendingDeletion*>(
        AxAlloc(m_Allocator, sizeof(PendingDeletion) * m_PendingCapacity));
    if (!m_PendingDeletions) {
        m_Textures.Shutdown();
        m_Meshes.Shutdown();
        m_Shaders.Shutdown();
        m_Materials.Shutdown();
        m_Models.Shutdown();
        return false;
    }
    m_PendingCount = 0;

    // Initialize texture path cache for deduplication
    m_TexturePathCacheCapacity = TextureCapacity * 2; // Load factor ~0.5
    m_TexturePathCache = static_cast<TexturePathCacheEntry*>(
        AxAlloc(m_Allocator, sizeof(TexturePathCacheEntry) * m_TexturePathCacheCapacity));
    if (!m_TexturePathCache) {
        AxFree(m_Allocator, m_PendingDeletions);
        m_Textures.Shutdown();
        m_Meshes.Shutdown();
        m_Shaders.Shutdown();
        m_Materials.Shutdown();
        m_Models.Shutdown();
        return false;
    }
    memset(m_TexturePathCache, 0, sizeof(TexturePathCacheEntry) * m_TexturePathCacheCapacity);
    m_TexturePathCacheCount = 0;

    m_Initialized = true;
    return true;
}

void ResourceSystem::Shutdown()
{
    if (!m_Initialized) {
        return;
    }

    // Process all pending deletions
    ProcessPendingReleases();

    // Clean up any remaining resources in slots (force destroy)
    // Models (must be before textures/meshes/materials - they reference those)
    for (uint32_t i = 1; i < m_Models.GetCapacity(); ++i) {
        ResourceSlot<AxModelData>* Slot = m_Models.GetSlotByIndex(i);
        if (Slot && Slot->InUse && Slot->Data) {
            // Release child resources (textures, meshes, materials)
            // Note: We don't call ReleaseModel here because that would decrement
            // refcounts that might go below 0. Instead, we just free the data.
            AxFree(m_Allocator, Slot->Data);
            Slot->Data = nullptr;
        }
    }

    // Textures
    for (uint32_t i = 1; i < m_Textures.GetCapacity(); ++i) {
        ResourceSlot<AxTexture>* Slot = m_Textures.GetSlotByIndex(i);
        if (Slot && Slot->InUse && Slot->Data) {
            if (m_RenderAPI) {
                m_RenderAPI->DestroyTexture(Slot->Data);
            }
            AxFree(m_Allocator, Slot->Data);
            Slot->Data = nullptr;
        }
    }

    // Shaders
    for (uint32_t i = 1; i < m_Shaders.GetCapacity(); ++i) {
        ResourceSlot<AxShaderData>* Slot = m_Shaders.GetSlotByIndex(i);
        if (Slot && Slot->InUse && Slot->Data) {
            if (m_RenderAPI && Slot->Data->ShaderHandle != 0) {
                m_RenderAPI->DestroyProgram(Slot->Data->ShaderHandle);
            }
            AxFree(m_Allocator, Slot->Data);
            Slot->Data = nullptr;
        }
    }

    // Meshes
    for (uint32_t i = 1; i < m_Meshes.GetCapacity(); ++i) {
        ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlotByIndex(i);
        if (Slot && Slot->InUse && Slot->Data) {
            // GPU cleanup would go here if we had DestroyMesh
            AxFree(m_Allocator, Slot->Data);
            Slot->Data = nullptr;
        }
    }

    // Materials
    for (uint32_t i = 1; i < m_Materials.GetCapacity(); ++i) {
        ResourceSlot<AxMaterialDesc>* Slot = m_Materials.GetSlotByIndex(i);
        if (Slot && Slot->InUse && Slot->Data) {
            // Free custom data if present
            if (Slot->Data->Type == AX_MATERIAL_TYPE_CUSTOM &&
                Slot->Data->Custom.CustomData != nullptr) {
                AxFree(m_Allocator, Slot->Data->Custom.CustomData);
            }
            AxFree(m_Allocator, Slot->Data);
            Slot->Data = nullptr;
        }
    }

    // Free pending deletion queue
    if (m_PendingDeletions) {
        AxFree(m_Allocator, m_PendingDeletions);
        m_PendingDeletions = nullptr;
    }
    m_PendingCount = 0;
    m_PendingCapacity = 0;

    // Free texture path cache
    if (m_TexturePathCache) {
        AxFree(m_Allocator, m_TexturePathCache);
        m_TexturePathCache = nullptr;
    }
    m_TexturePathCacheCount = 0;
    m_TexturePathCacheCapacity = 0;

    // Shutdown slot arrays (this does NOT destroy actual resources)
    // Resource destruction should happen via ProcessPendingReleases
    m_Models.Shutdown();
    m_Textures.Shutdown();
    m_Meshes.Shutdown();
    m_Shaders.Shutdown();
    m_Materials.Shutdown();

    m_RenderAPI = nullptr;
    m_PlatformAPI = nullptr;
    m_Registry = nullptr;
    m_Allocator = nullptr;
    m_Initialized = false;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

char* ResourceSystem::ReadFileToString(std::string_view Path, uint64_t* OutSize)
{
    if (!m_PlatformAPI || !m_PlatformAPI->FileAPI) {
        AX_LOG(ERROR, "PlatformAPI not available");
        return nullptr;
    }

    struct AxPlatformFileAPI* FileAPI = m_PlatformAPI->FileAPI;

    AxFile File = FileAPI->OpenForRead(Path.data());
    if (!FileAPI->IsValid(File)) {
        AX_LOG(ERROR, "Failed to open file: %.*s", static_cast<int>(Path.size()), Path.data());
        return nullptr;
    }

    uint64_t FileSize = FileAPI->Size(File);
    if (FileSize == 0) {
        AX_LOG(ERROR, "File is empty: %.*s", static_cast<int>(Path.size()), Path.data());
        FileAPI->Close(File);
        return nullptr;
    }

    // Allocate buffer with extra byte for null terminator
    char* Buffer = static_cast<char*>(AxAlloc(m_Allocator, FileSize + 1));
    if (!Buffer) {
        AX_LOG(ERROR, "Failed to allocate buffer for file: %.*s", static_cast<int>(Path.size()), Path.data());
        FileAPI->Close(File);
        return nullptr;
    }

    uint64_t BytesRead = FileAPI->Read(File, Buffer, static_cast<uint32_t>(FileSize));
    FileAPI->Close(File);

    if (BytesRead != FileSize) {
        AX_LOG(ERROR, "Failed to read file: %.*s", static_cast<int>(Path.size()), Path.data());
        AxFree(m_Allocator, Buffer);
        return nullptr;
    }

    // Null-terminate
    Buffer[FileSize] = '\0';

    if (OutSize) {
        *OutSize = FileSize;
    }

    return Buffer;
}


//=============================================================================
// Texture Path Cache (for deduplication)
//=============================================================================

uint32_t ResourceSystem::HashPath(std::string_view NormalizedPath) const
{
    // FNV-1a hash
    uint32_t Hash = 2166136261u;
    for (char c : NormalizedPath) {
        Hash ^= static_cast<uint32_t>(c);
        Hash *= 16777619u;
    }
    return Hash;
}

AxTextureHandle ResourceSystem::TexturePathCacheFind(std::string_view NormalizedPath)
{
    if (!m_TexturePathCache || m_TexturePathCacheCapacity == 0) {
        return AX_INVALID_HANDLE;
    }

    uint32_t Hash = HashPath(NormalizedPath);
    uint32_t Index = Hash % m_TexturePathCacheCapacity;
    uint32_t StartIndex = Index;

    // Linear probing
    do {
        TexturePathCacheEntry* Entry = &m_TexturePathCache[Index];

        if (!Entry->InUse) {
            // Empty slot - path not in cache
            return AX_INVALID_HANDLE;
        }

        if (Entry->Hash == Hash && strcmp(Entry->Path, NormalizedPath.data()) == 0) {
            // Found it - verify the handle is still valid
            if (IsTextureValid(Entry->Handle)) {
                return Entry->Handle;
            } else {
                // Handle is stale, remove from cache
                Entry->InUse = false;
                m_TexturePathCacheCount--;
                return AX_INVALID_HANDLE;
            }
        }

        // Collision, try next slot
        Index = (Index + 1) % m_TexturePathCacheCapacity;
    } while (Index != StartIndex);

    return AX_INVALID_HANDLE;
}

void ResourceSystem::TexturePathCacheInsert(std::string_view NormalizedPath, AxTextureHandle Handle)
{
    if (!m_TexturePathCache || m_TexturePathCacheCapacity == 0) {
        return;
    }

    // Check load factor - if too high, don't insert (simple approach)
    if (m_TexturePathCacheCount >= m_TexturePathCacheCapacity / 2) {
        // Cache is getting full - skip insertion rather than resize
        // A more complete implementation would resize here
        return;
    }

    uint32_t Hash = HashPath(NormalizedPath);
    uint32_t Index = Hash % m_TexturePathCacheCapacity;
    uint32_t StartIndex = Index;

    // Linear probing to find empty slot
    do {
        TexturePathCacheEntry* Entry = &m_TexturePathCache[Index];

        if (!Entry->InUse) {
            // Found empty slot
            Entry->Hash = Hash;
            strncpy(Entry->Path, NormalizedPath.data(), sizeof(Entry->Path) - 1);
            Entry->Path[sizeof(Entry->Path) - 1] = '\0';
            Entry->Handle = Handle;
            Entry->InUse = true;
            m_TexturePathCacheCount++;
            return;
        }

        // Slot occupied, try next
        Index = (Index + 1) % m_TexturePathCacheCapacity;
    } while (Index != StartIndex);

    // Cache is full - shouldn't happen with proper load factor check
}

void ResourceSystem::TexturePathCacheRemove(AxTextureHandle Handle)
{
    if (!m_TexturePathCache || m_TexturePathCacheCapacity == 0) {
        return;
    }

    // Linear scan to find the entry with this handle
    // (We could optimize with a reverse lookup, but textures aren't deleted often)
    for (uint32_t i = 0; i < m_TexturePathCacheCapacity; ++i) {
        TexturePathCacheEntry* Entry = &m_TexturePathCache[i];
        if (Entry->InUse &&
            Entry->Handle.Index == Handle.Index &&
            Entry->Handle.Generation == Handle.Generation) {
            Entry->InUse = false;
            m_TexturePathCacheCount--;
            return;
        }
    }
}

//=============================================================================
// Texture Loading and Management
//=============================================================================

AxTextureHandle ResourceSystem::LoadTexture(std::string_view Path,
                                             const struct AxTextureLoadOptions* Options)
{
    if (!m_Initialized) {
        AX_LOG(ERROR, "Not initialized");
        return AX_INVALID_HANDLE;
    }

    if (Path.empty()) {
        AX_LOG(ERROR, "Invalid texture path");
        return AX_INVALID_HANDLE;
    }

    // Check if file exists
    if (m_PlatformAPI && m_PlatformAPI->PathAPI) {
        if (!m_PlatformAPI->PathAPI->FileExists(Path.data())) {
            AX_LOG(ERROR, "Texture file not found: %.*s", static_cast<int>(Path.size()), Path.data());
            return AX_INVALID_HANDLE;
        }
    }

    // Normalize path for cache lookup
    std::string_view NormalizedPath = Path;
    if (m_PlatformAPI && m_PlatformAPI->PathAPI && m_PlatformAPI->PathAPI->Normalize) {
        NormalizedPath = m_PlatformAPI->PathAPI->Normalize(Path.data());
        AX_LOG(DEBUG, "Normalized '%.*s' -> '%.*s'", static_cast<int>(Path.size()), Path.data(), static_cast<int>(NormalizedPath.size()), NormalizedPath.data());
    } else {
        AX_LOG(WARNING, "Normalize function not available, using raw path");
    }

    // Check cache for existing texture
    AxTextureHandle CachedHandle = TexturePathCacheFind(NormalizedPath);
    if (AX_HANDLE_IS_VALID(CachedHandle)) {
        // Cache hit - acquire and return existing handle
        return AcquireTexture(CachedHandle);
    }

    // Cache miss - load from disk
    // Load image data using stb_image
    int Width, Height, Channels;
    int DesiredChannels = 0; // Auto-detect channels

    unsigned char* Pixels = stbi_load(Path.data(), &Width, &Height, &Channels, DesiredChannels);
    if (!Pixels) {
        AX_LOG(ERROR, "Failed to load texture: %.*s - %s",
                static_cast<int>(Path.size()), Path.data(), stbi_failure_reason());
        return AX_INVALID_HANDLE;
    }

    // Allocate a slot for the texture
    AxTextureHandle Handle = m_Textures.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        AX_LOG(ERROR, "Failed to allocate texture slot");
        stbi_image_free(Pixels);
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxTexture>* Slot = m_Textures.GetSlot(Handle);
    if (!Slot) {
        AX_LOG(ERROR, "Failed to get texture slot");
        stbi_image_free(Pixels);
        return AX_INVALID_HANDLE;
    }

    // Allocate texture data structure
    AxTexture* Texture = static_cast<AxTexture*>(AxAlloc(m_Allocator, sizeof(AxTexture)));
    if (!Texture) {
        AX_LOG(ERROR, "Failed to allocate texture data");
        m_Textures.Free(Handle.Index);
        stbi_image_free(Pixels);
        return AX_INVALID_HANDLE;
    }

    // Initialize texture data
    memset(Texture, 0, sizeof(AxTexture));
    Texture->Width = static_cast<uint32_t>(Width);
    Texture->Height = static_cast<uint32_t>(Height);
    Texture->Channels = static_cast<uint32_t>(Channels);
    Texture->IsSRGB = Options ? Options->IsSRGB : false;

    // Default sampler settings
    Texture->WrapS = 0x2901; // GL_REPEAT
    Texture->WrapT = 0x2901; // GL_REPEAT
    Texture->MagFilter = 0x2601; // GL_LINEAR
    Texture->MinFilter = 0x2703; // GL_LINEAR_MIPMAP_LINEAR

    // Upload to GPU using RenderAPI
    if (m_RenderAPI) {
        m_RenderAPI->InitTexture(Texture, Pixels);
    }

    // Free the stb_image pixel data
    stbi_image_free(Pixels);

    // Store texture in slot
    Slot->Data = Texture;

    // Add to path cache for future deduplication
    TexturePathCacheInsert(NormalizedPath, Handle);

    AX_LOG(INFO, "Loaded texture [%u:%u] - %.*s (%dx%d, %d channels)",
           Handle.Index, Handle.Generation, static_cast<int>(Path.size()), Path.data(), Width, Height, Channels);

    return Handle;
}

AxTextureHandle ResourceSystem::CreateTextureSlot()
{
    return m_Textures.Allocate();
}

bool ResourceSystem::IsTextureValid(AxTextureHandle Handle) const
{
    return m_Textures.IsValid(Handle);
}

ResourceSlot<AxTexture>* ResourceSystem::GetTextureSlot(AxTextureHandle Handle)
{
    return m_Textures.GetSlot(Handle);
}

const ResourceSlot<AxTexture>* ResourceSystem::GetTextureSlot(AxTextureHandle Handle) const
{
    return m_Textures.GetSlot(Handle);
}

AxTextureHandle ResourceSystem::AcquireTexture(AxTextureHandle Handle)
{
    ResourceSlot<AxTexture>* Slot = m_Textures.GetSlot(Handle);
    if (!Slot) {
        return AX_INVALID_HANDLE;
    }
    Slot->RefCount++;
    return Handle;
}

void ResourceSystem::ReleaseTexture(AxTextureHandle Handle)
{
    ResourceSlot<AxTexture>* Slot = m_Textures.GetSlot(Handle);
    if (!Slot || Slot->RefCount == 0) {
        return;
    }

    Slot->RefCount--;
    if (Slot->RefCount == 0) {
        // Queue for deferred deletion
        QueueForDeletion(ResourceType::Texture, Handle.Index, Handle.Generation);
    }
}

uint32_t ResourceSystem::GetTextureRefCount(AxTextureHandle Handle) const
{
    const ResourceSlot<AxTexture>* Slot = m_Textures.GetSlot(Handle);
    return Slot ? Slot->RefCount : 0;
}

uint32_t ResourceSystem::GetTextureCount() const
{
    return m_Textures.GetUsedCount();
}

//=============================================================================
// Mesh Loading and Management
//=============================================================================

AxMeshHandle ResourceSystem::LoadMesh(std::string_view Path,
                                       const struct AxMeshLoadOptions* Options)
{
    if (!m_Initialized) {
        AX_LOG(ERROR, "Not initialized");
        return AX_INVALID_HANDLE;
    }

    if (Path.empty()) {
        AX_LOG(ERROR, "Invalid mesh path");
        return AX_INVALID_HANDLE;
    }

    // Check if file exists
    if (m_PlatformAPI && m_PlatformAPI->PathAPI) {
        if (!m_PlatformAPI->PathAPI->FileExists(Path.data())) {
            AX_LOG(ERROR, "Mesh file not found: %.*s", static_cast<int>(Path.size()), Path.data());
            return AX_INVALID_HANDLE;
        }
    }

    // Parse GLTF file
    cgltf_options ParseOptions;
    memset(&ParseOptions, 0, sizeof(ParseOptions));

    cgltf_data* Data = nullptr;
    cgltf_result Result = cgltf_parse_file(&ParseOptions, Path.data(), &Data);
    if (Result != cgltf_result_success) {
        AX_LOG(ERROR, "Failed to parse GLTF: %.*s", static_cast<int>(Path.size()), Path.data());
        return AX_INVALID_HANDLE;
    }

    // Load buffers
    Result = cgltf_load_buffers(&ParseOptions, Data, Path.data());
    if (Result != cgltf_result_success) {
        AX_LOG(ERROR, "Failed to load GLTF buffers: %.*s", static_cast<int>(Path.size()), Path.data());
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    // Check if there's at least one mesh
    if (Data->meshes_count == 0) {
        AX_LOG(ERROR, "No meshes in GLTF file: %.*s", static_cast<int>(Path.size()), Path.data());
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    // Get the first mesh's first primitive
    cgltf_mesh* GltfMesh = &Data->meshes[0];
    if (GltfMesh->primitives_count == 0) {
        AX_LOG(ERROR, "No primitives in mesh: %.*s", static_cast<int>(Path.size()), Path.data());
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    cgltf_primitive* Primitive = &GltfMesh->primitives[0];

    // Find position, normal, texcoord, and tangent accessors
    cgltf_accessor* PositionAccessor = nullptr;
    cgltf_accessor* NormalAccessor = nullptr;
    cgltf_accessor* TexCoordAccessor = nullptr;
    cgltf_accessor* TangentAccessor = nullptr;

    for (size_t i = 0; i < Primitive->attributes_count; ++i) {
        cgltf_attribute* Attr = &Primitive->attributes[i];
        switch (Attr->type) {
            case cgltf_attribute_type_position:
                PositionAccessor = Attr->data;
                break;
            case cgltf_attribute_type_normal:
                NormalAccessor = Attr->data;
                break;
            case cgltf_attribute_type_texcoord:
                if (Attr->index == 0) {
                    TexCoordAccessor = Attr->data;
                }
                break;
            case cgltf_attribute_type_tangent:
                TangentAccessor = Attr->data;
                break;
            default:
                break;
        }
    }

    if (!PositionAccessor) {
        AX_LOG(ERROR, "No position data in mesh: %.*s", static_cast<int>(Path.size()), Path.data());
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    // Get scale factor
    float Scale = Options ? Options->Scale : 1.0f;
    if (Scale == 0.0f) {
        Scale = 1.0f;
    }

    // Create vertex array
    uint32_t VertexCount = static_cast<uint32_t>(PositionAccessor->count);
    AxVertex* Vertices = static_cast<AxVertex*>(AxAlloc(m_Allocator, sizeof(AxVertex) * VertexCount));
    if (!Vertices) {
        AX_LOG(ERROR, "Failed to allocate vertices for mesh: %.*s", static_cast<int>(Path.size()), Path.data());
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    // Initialize vertices
    memset(Vertices, 0, sizeof(AxVertex) * VertexCount);

    // Read positions
    for (uint32_t i = 0; i < VertexCount; ++i) {
        cgltf_accessor_read_float(PositionAccessor, i, &Vertices[i].Position.X, 3);
        Vertices[i].Position.X *= Scale;
        Vertices[i].Position.Y *= Scale;
        Vertices[i].Position.Z *= Scale;
    }

    // Read normals
    if (NormalAccessor) {
        for (uint32_t i = 0; i < VertexCount; ++i) {
            cgltf_accessor_read_float(NormalAccessor, i, &Vertices[i].Normal.X, 3);
        }
    }

    // Read texture coordinates
    if (TexCoordAccessor) {
        for (uint32_t i = 0; i < VertexCount; ++i) {
            cgltf_accessor_read_float(TexCoordAccessor, i, &Vertices[i].TexCoord.X, 2);
        }
    }

    // Read tangents
    if (TangentAccessor) {
        for (uint32_t i = 0; i < VertexCount; ++i) {
            cgltf_accessor_read_float(TangentAccessor, i, &Vertices[i].Tangent.X, 4);
        }
    } else if (Options && Options->CalculateTangents && NormalAccessor && TexCoordAccessor) {
        // Simple default tangents
        for (uint32_t i = 0; i < VertexCount; ++i) {
            Vertices[i].Tangent.X = 1.0f;
            Vertices[i].Tangent.Y = 0.0f;
            Vertices[i].Tangent.Z = 0.0f;
            Vertices[i].Tangent.W = 1.0f;
        }
    }

    // Read indices
    uint32_t IndexCount = 0;
    uint32_t* Indices = nullptr;

    if (Primitive->indices) {
        IndexCount = static_cast<uint32_t>(Primitive->indices->count);
        Indices = static_cast<uint32_t*>(AxAlloc(m_Allocator, sizeof(uint32_t) * IndexCount));
        if (!Indices) {
            AX_LOG(ERROR, "Failed to allocate indices");
            AxFree(m_Allocator, Vertices);
            cgltf_free(Data);
            return AX_INVALID_HANDLE;
        }

        for (uint32_t i = 0; i < IndexCount; ++i) {
            Indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(Primitive->indices, i));
        }

        // Optionally flip winding order
        if (Options && Options->FlipWindingOrder) {
            for (uint32_t i = 0; i + 2 < IndexCount; i += 3) {
                uint32_t Temp = Indices[i + 1];
                Indices[i + 1] = Indices[i + 2];
                Indices[i + 2] = Temp;
            }
        }
    }

    // Allocate a slot for the mesh
    AxMeshHandle Handle = m_Meshes.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        AX_LOG(ERROR, "Failed to allocate mesh slot");
        AxFree(m_Allocator, Vertices);
        if (Indices) {
            AxFree(m_Allocator, Indices);
        }
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlot(Handle);
    if (!Slot) {
        AX_LOG(ERROR, "Failed to get mesh slot");
        AxFree(m_Allocator, Vertices);
        if (Indices) {
            AxFree(m_Allocator, Indices);
        }
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    // Allocate mesh data structure
    AxMesh* Mesh = static_cast<AxMesh*>(AxAlloc(m_Allocator, sizeof(AxMesh)));
    if (!Mesh) {
        AX_LOG(ERROR, "Failed to allocate mesh data");
        m_Meshes.Free(Handle.Index);
        AxFree(m_Allocator, Vertices);
        if (Indices) {
            AxFree(m_Allocator, Indices);
        }
        cgltf_free(Data);
        return AX_INVALID_HANDLE;
    }

    // Initialize mesh data
    memset(Mesh, 0, sizeof(AxMesh));
    Mesh->IndexCount = IndexCount;
    Mesh->VertexOffset = 0;
    Mesh->IndexOffset = 0;

    // Copy mesh name
    if (GltfMesh->name) {
        strncpy(Mesh->Name, GltfMesh->name, sizeof(Mesh->Name) - 1);
    } else {
        strncpy(Mesh->Name, Path.data(), sizeof(Mesh->Name) - 1);
    }

    // Upload to GPU using RenderAPI
    if (m_RenderAPI) {
        m_RenderAPI->InitMesh(Mesh, Vertices, Indices, VertexCount, IndexCount);
    }

    // Free temporary vertex/index data
    AxFree(m_Allocator, Vertices);
    if (Indices) {
        AxFree(m_Allocator, Indices);
    }

    // Free GLTF data
    cgltf_free(Data);

    // Store mesh in slot
    Slot->Data = Mesh;

    AX_LOG(INFO, "Loaded mesh [%u:%u] - %.*s (%u vertices, %u indices)",
           Handle.Index, Handle.Generation, static_cast<int>(Path.size()), Path.data(), VertexCount, IndexCount);

    return Handle;
}

AxMeshHandle ResourceSystem::CreateMeshSlot()
{
    return m_Meshes.Allocate();
}

bool ResourceSystem::IsMeshValid(AxMeshHandle Handle) const
{
    return m_Meshes.IsValid(Handle);
}

ResourceSlot<AxMesh>* ResourceSystem::GetMeshSlot(AxMeshHandle Handle)
{
    return m_Meshes.GetSlot(Handle);
}

const ResourceSlot<AxMesh>* ResourceSystem::GetMeshSlot(AxMeshHandle Handle) const
{
    return m_Meshes.GetSlot(Handle);
}

AxMeshHandle ResourceSystem::AcquireMesh(AxMeshHandle Handle)
{
    ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlot(Handle);
    if (!Slot) {
        return AX_INVALID_HANDLE;
    }
    Slot->RefCount++;
    return Handle;
}

void ResourceSystem::ReleaseMesh(AxMeshHandle Handle)
{
    ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlot(Handle);
    if (!Slot || Slot->RefCount == 0) {
        return;
    }

    Slot->RefCount--;
    if (Slot->RefCount == 0) {
        QueueForDeletion(ResourceType::Mesh, Handle.Index, Handle.Generation);
    }
}

uint32_t ResourceSystem::GetMeshRefCount(AxMeshHandle Handle) const
{
    const ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlot(Handle);
    return Slot ? Slot->RefCount : 0;
}

uint32_t ResourceSystem::GetMeshCount() const
{
    return m_Meshes.GetUsedCount();
}

//=============================================================================
// Shader Loading and Management
//=============================================================================

AxShaderHandle ResourceSystem::LoadShader(std::string_view VertexPath,
                                           std::string_view FragmentPath,
                                           const struct AxShaderLoadOptions* Options)
{
    if (!m_Initialized) {
        AX_LOG(ERROR, "Not initialized");
        return AX_INVALID_HANDLE;
    }

    if (VertexPath.empty()) {
        AX_LOG(ERROR, "Invalid vertex shader path");
        return AX_INVALID_HANDLE;
    }

    if (FragmentPath.empty()) {
        AX_LOG(ERROR, "Invalid fragment shader path");
        return AX_INVALID_HANDLE;
    }

    if (!m_RenderAPI) {
        AX_LOG(ERROR, "RenderAPI not available for shader compilation");
        return AX_INVALID_HANDLE;
    }

    // Check if files exist
    if (m_PlatformAPI && m_PlatformAPI->PathAPI) {
        if (!m_PlatformAPI->PathAPI->FileExists(VertexPath.data())) {
            AX_LOG(ERROR, "Vertex shader not found: %.*s", static_cast<int>(VertexPath.size()), VertexPath.data());
            return AX_INVALID_HANDLE;
        }
        if (!m_PlatformAPI->PathAPI->FileExists(FragmentPath.data())) {
            AX_LOG(ERROR, "Fragment shader not found: %.*s", static_cast<int>(FragmentPath.size()), FragmentPath.data());
            return AX_INVALID_HANDLE;
        }
    }

    // Read shader source files
    uint64_t VertexSize = 0;
    uint64_t FragmentSize = 0;

    std::string_view VertexSource = ReadFileToString(VertexPath, &VertexSize);
    if (!VertexSource.data() || VertexSource[0] == '\0') {
        return AX_INVALID_HANDLE;
    }

    std::string_view FragmentSource = ReadFileToString(FragmentPath, &FragmentSize);
    if (!FragmentSource.data() || FragmentSource[0] == '\0') {
        return AX_INVALID_HANDLE;
    }

    // Handle optional defines
    std::string_view HeaderCode = "";
    if (Options && Options->Defines && Options->Defines[0] != '\0') {
        // TODO: Parse semicolon-separated defines and format as #define statements
        // For now, pass empty header
    }

    // Compile shader program
    uint32_t ShaderProgram = m_RenderAPI->CreateProgram(HeaderCode.data(), VertexSource.data(), FragmentSource.data());

    if (ShaderProgram == 0) {
        AX_LOG(ERROR, "Failed to compile shader: %.*s, %.*s",
                static_cast<int>(VertexPath.size()), VertexPath.data(),
                static_cast<int>(FragmentPath.size()), FragmentPath.data());
        return AX_INVALID_HANDLE;
    }

    // Allocate a slot for the shader
    AxShaderHandle Handle = m_Shaders.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        AX_LOG(ERROR, "Failed to allocate shader slot");
        m_RenderAPI->DestroyProgram(ShaderProgram);
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxShaderData>* Slot = m_Shaders.GetSlot(Handle);
    if (!Slot) {
        AX_LOG(ERROR, "Failed to get shader slot");
        m_RenderAPI->DestroyProgram(ShaderProgram);
        return AX_INVALID_HANDLE;
    }

    // Allocate shader data structure
    AxShaderData* ShaderData = static_cast<AxShaderData*>(AxAlloc(m_Allocator, sizeof(AxShaderData)));
    if (!ShaderData) {
        AX_LOG(ERROR, "Failed to allocate shader data");
        m_Shaders.Free(Handle.Index);
        m_RenderAPI->DestroyProgram(ShaderProgram);
        return AX_INVALID_HANDLE;
    }

    // Initialize shader data
    memset(ShaderData, 0, sizeof(AxShaderData));
    ShaderData->ShaderHandle = ShaderProgram;

    // Get attribute and uniform locations
    m_RenderAPI->GetAttributeLocations(ShaderProgram, ShaderData);

    // Store shader in slot
    Slot->Data = ShaderData;

    AX_LOG(INFO, "Loaded shader [%u:%u] - %.*s, %.*s (Program=%u)",
           Handle.Index, Handle.Generation, static_cast<int>(VertexPath.size()), VertexPath.data(),
           static_cast<int>(FragmentPath.size()), FragmentPath.data(), ShaderProgram);

    return Handle;
}

AxShaderHandle ResourceSystem::CreateShaderSlot()
{
    return m_Shaders.Allocate();
}

bool ResourceSystem::IsShaderValid(AxShaderHandle Handle) const
{
    return m_Shaders.IsValid(Handle);
}

ResourceSlot<AxShaderData>* ResourceSystem::GetShaderSlot(AxShaderHandle Handle)
{
    return m_Shaders.GetSlot(Handle);
}

const ResourceSlot<AxShaderData>* ResourceSystem::GetShaderSlot(AxShaderHandle Handle) const
{
    return m_Shaders.GetSlot(Handle);
}

AxShaderHandle ResourceSystem::AcquireShader(AxShaderHandle Handle)
{
    ResourceSlot<AxShaderData>* Slot = m_Shaders.GetSlot(Handle);
    if (!Slot) {
        return AX_INVALID_HANDLE;
    }
    Slot->RefCount++;
    return Handle;
}

void ResourceSystem::ReleaseShader(AxShaderHandle Handle)
{
    ResourceSlot<AxShaderData>* Slot = m_Shaders.GetSlot(Handle);
    if (!Slot || Slot->RefCount == 0) {
        return;
    }

    Slot->RefCount--;
    if (Slot->RefCount == 0) {
        QueueForDeletion(ResourceType::Shader, Handle.Index, Handle.Generation);
    }
}

uint32_t ResourceSystem::GetShaderRefCount(AxShaderHandle Handle) const
{
    const ResourceSlot<AxShaderData>* Slot = m_Shaders.GetSlot(Handle);
    return Slot ? Slot->RefCount : 0;
}

uint32_t ResourceSystem::GetShaderCount() const
{
    return m_Shaders.GetUsedCount();
}

//=============================================================================
// Material Management
//=============================================================================

AxMaterialHandle ResourceSystem::CreateMaterial(const struct AxMaterialDesc* Desc)
{
    if (!m_Initialized) {
        AX_LOG(ERROR, "Not initialized");
        return AX_INVALID_HANDLE;
    }

    if (!Desc) {
        AX_LOG(ERROR, "NULL material description");
        return AX_INVALID_HANDLE;
    }

    // Allocate a slot for the material
    AxMaterialHandle Handle = m_Materials.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        AX_LOG(ERROR, "Failed to allocate material slot");
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxMaterialDesc>* Slot = m_Materials.GetSlot(Handle);
    if (!Slot) {
        AX_LOG(ERROR, "Failed to get material slot");
        return AX_INVALID_HANDLE;
    }

    // Allocate material data structure
    AxMaterialDesc* Material = static_cast<AxMaterialDesc*>(AxAlloc(m_Allocator, sizeof(AxMaterialDesc)));
    if (!Material) {
        AX_LOG(ERROR, "Failed to allocate material data");
        m_Materials.Free(Handle.Index);
        return AX_INVALID_HANDLE;
    }

    // Copy material description
    memcpy(Material, Desc, sizeof(AxMaterialDesc));

    // If custom material with custom data, make a deep copy
    if (Desc->Type == AX_MATERIAL_TYPE_CUSTOM &&
        Desc->Custom.CustomData != nullptr &&
        Desc->Custom.CustomDataSize > 0) {
        void* CustomDataCopy = AxAlloc(m_Allocator, Desc->Custom.CustomDataSize);
        if (!CustomDataCopy) {
            AX_LOG(ERROR, "Failed to allocate custom data");
            AxFree(m_Allocator, Material);
            m_Materials.Free(Handle.Index);
            return AX_INVALID_HANDLE;
        }
        memcpy(CustomDataCopy, Desc->Custom.CustomData, Desc->Custom.CustomDataSize);
        Material->Custom.CustomData = CustomDataCopy;
    }

    // Store material in slot
    Slot->Data = Material;

    AX_LOG(DEBUG, "Created material [%u:%u] - %s (Type=%s)",
           Handle.Index, Handle.Generation, Material->Name,
           Material->Type == AX_MATERIAL_TYPE_PBR ? "PBR" : "Custom");

    return Handle;
}

AxMaterialHandle ResourceSystem::CreateMaterialSlot()
{
    return m_Materials.Allocate();
}

bool ResourceSystem::IsMaterialValid(AxMaterialHandle Handle) const
{
    return m_Materials.IsValid(Handle);
}

ResourceSlot<AxMaterialDesc>* ResourceSystem::GetMaterialSlot(AxMaterialHandle Handle)
{
    return m_Materials.GetSlot(Handle);
}

const ResourceSlot<AxMaterialDesc>* ResourceSystem::GetMaterialSlot(AxMaterialHandle Handle) const
{
    return m_Materials.GetSlot(Handle);
}

AxMaterialHandle ResourceSystem::AcquireMaterial(AxMaterialHandle Handle)
{
    ResourceSlot<AxMaterialDesc>* Slot = m_Materials.GetSlot(Handle);
    if (!Slot) {
        return AX_INVALID_HANDLE;
    }
    Slot->RefCount++;
    return Handle;
}

void ResourceSystem::ReleaseMaterial(AxMaterialHandle Handle)
{
    ResourceSlot<AxMaterialDesc>* Slot = m_Materials.GetSlot(Handle);
    if (!Slot || Slot->RefCount == 0) {
        return;
    }

    Slot->RefCount--;
    if (Slot->RefCount == 0) {
        QueueForDeletion(ResourceType::Material, Handle.Index, Handle.Generation);
    }
}

uint32_t ResourceSystem::GetMaterialRefCount(AxMaterialHandle Handle) const
{
    const ResourceSlot<AxMaterialDesc>* Slot = m_Materials.GetSlot(Handle);
    return Slot ? Slot->RefCount : 0;
}

uint32_t ResourceSystem::GetMaterialCount() const
{
    return m_Materials.GetUsedCount();
}

//=============================================================================
// Model Loading (Handle-based - Phase 6)
//=============================================================================

bool ResourceSystem::LoadModelInternal(std::string_view Path, struct AxModelData* OutModel)
{
    // Initialize output model to zero
    memset(OutModel, 0, sizeof(AxModelData));

    // Initialize all material indices to -1 (no material)
    for (uint32_t i = 0; i < AX_MODEL_MAX_MESHES; ++i) {
        OutModel->MeshMaterials[i] = -1;
    }

    // Check if file exists
    if (m_PlatformAPI && m_PlatformAPI->PathAPI) {
        if (!m_PlatformAPI->PathAPI->FileExists(Path.data())) {
            AX_LOG(ERROR, "Model file not found: %.*s", static_cast<int>(Path.size()), Path.data());
            return false;
        }
    }

    // Extract base path and filename using platform path API
    if (m_PlatformAPI && m_PlatformAPI->PathAPI) {
        const char* Base = m_PlatformAPI->PathAPI->BasePath(Path.data());
        strncpy(OutModel->BasePath, Base ? Base : "", sizeof(OutModel->BasePath) - 1);
        OutModel->BasePath[sizeof(OutModel->BasePath) - 1] = '\0';

        const char* FN = m_PlatformAPI->PathAPI->FileName(Path.data());
        strncpy(OutModel->Name, FN ? FN : Path.data(), sizeof(OutModel->Name) - 1);
    } else {
        OutModel->BasePath[0] = '\0';
        strncpy(OutModel->Name, Path.data(), sizeof(OutModel->Name) - 1);
    }
    OutModel->Name[sizeof(OutModel->Name) - 1] = '\0';

    // Parse GLTF file
    cgltf_options ParseOptions;
    memset(&ParseOptions, 0, sizeof(ParseOptions));

    cgltf_data* Data = nullptr;
    cgltf_result Result = cgltf_parse_file(&ParseOptions, Path.data(), &Data);
    if (Result != cgltf_result_success) {
        AX_LOG(ERROR, "Failed to parse GLTF: %.*s", static_cast<int>(Path.size()), Path.data());
        return false;
    }

    // Load buffers
    Result = cgltf_load_buffers(&ParseOptions, Data, Path.data());
    if (Result != cgltf_result_success) {
        AX_LOG(ERROR, "Failed to load GLTF buffers: %.*s", static_cast<int>(Path.size()), Path.data());
        cgltf_free(Data);
        return false;
    }

    // Local texture cache for deduplication within this model
    // Maps cgltf_image* to texture index in OutModel->Textures
    struct ImageCacheEntry {
        cgltf_image* Image;
        int32_t TextureIndex;
    };
    ImageCacheEntry ImageCache[AX_MODEL_MAX_TEXTURES];
    uint32_t ImageCacheCount = 0;
    uint32_t ImageCacheHits = 0;
    uint32_t ImageCacheMisses = 0;

    // Helper lambda to find cached texture by image pointer
    auto FindCachedImage = [&](cgltf_image* Image) -> int32_t {
        for (uint32_t i = 0; i < ImageCacheCount; ++i) {
            if (ImageCache[i].Image == Image) {
                return ImageCache[i].TextureIndex;
            }
        }
        return -1;
    };

    // Helper lambda to cache a loaded texture
    auto CacheImage = [&](cgltf_image* Image, int32_t TextureIndex) {
        if (ImageCacheCount < AX_MODEL_MAX_TEXTURES) {
            ImageCache[ImageCacheCount].Image = Image;
            ImageCache[ImageCacheCount].TextureIndex = TextureIndex;
            ImageCacheCount++;
        }
    };

    // Helper lambda to load a texture from GLTF texture view
    // Returns the texture index in OutModel->Textures, or -1 on failure
    auto LoadGltfTexture = [&](cgltf_texture_view* TexView) -> int32_t {
        if (!TexView || !TexView->texture || !TexView->texture->image) {
            return -1;
        }

        cgltf_image* Image = TexView->texture->image;

        // Check local cache first (deduplicates within this model)
        int32_t CachedIndex = FindCachedImage(Image);
        if (CachedIndex >= 0) {
            ImageCacheHits++;
            return CachedIndex;
        }
        ImageCacheMisses++;

        if (Image->uri && Image->uri[0] != '\0') {
            // External texture file
            char TexturePath[512];
            snprintf(TexturePath, sizeof(TexturePath), "%s%s", OutModel->BasePath, Image->uri);

            AxTextureHandle TexHandle = LoadTexture(TexturePath, nullptr);
            if (AX_HANDLE_IS_VALID(TexHandle)) {
                int32_t TextureIndex = static_cast<int32_t>(OutModel->TextureCount);
                OutModel->Textures[OutModel->TextureCount] = TexHandle;
                OutModel->TextureCount++;
                CacheImage(Image, TextureIndex);
                return TextureIndex;
            } else {
                AX_LOG(ERROR, "Failed to load texture: %s", TexturePath);
                return -1;
            }
        } else if (Image->buffer_view) {
            // Embedded texture in buffer
            cgltf_buffer_view* BufferView = Image->buffer_view;
            cgltf_buffer* Buffer = BufferView->buffer;

            const unsigned char* ImageData = static_cast<const unsigned char*>(Buffer->data) + BufferView->offset;

            int Width, Height, Channels;
            unsigned char* Pixels = stbi_load_from_memory(ImageData, static_cast<int>(BufferView->size),
                                                           &Width, &Height, &Channels, 0);
            if (!Pixels) {
                AX_LOG(ERROR, "Failed to load embedded texture: %s",
                        stbi_failure_reason());
                return -1;
            }

            // Create texture slot
            AxTextureHandle TexHandle = m_Textures.Allocate();
            if (!AX_HANDLE_IS_VALID(TexHandle)) {
                stbi_image_free(Pixels);
                return -1;
            }

            ResourceSlot<AxTexture>* Slot = m_Textures.GetSlot(TexHandle);
            if (!Slot) {
                stbi_image_free(Pixels);
                return -1;
            }

            AxTexture* Texture = static_cast<AxTexture*>(AxAlloc(m_Allocator, sizeof(AxTexture)));
            if (!Texture) {
                stbi_image_free(Pixels);
                return -1;
            }

            memset(Texture, 0, sizeof(AxTexture));
            Texture->Width = Width;
            Texture->Height = Height;
            Texture->Channels = Channels;
            Texture->WrapS = 0x2901;    // GL_REPEAT
            Texture->WrapT = 0x2901;    // GL_REPEAT
            Texture->MagFilter = 0x2601; // GL_LINEAR
            Texture->MinFilter = 0x2703; // GL_LINEAR_MIPMAP_LINEAR

            if (m_RenderAPI) {
                m_RenderAPI->InitTexture(Texture, Pixels);
            }

            stbi_image_free(Pixels);

            Slot->Data = Texture;
            int32_t TextureIndex = static_cast<int32_t>(OutModel->TextureCount);
            OutModel->Textures[OutModel->TextureCount] = TexHandle;
            OutModel->TextureCount++;
            CacheImage(Image, TextureIndex);
            return TextureIndex;
        }

        return -1;
    };

    // Load all materials from the GLTF file, loading textures as needed
    for (size_t i = 0; i < Data->materials_count && OutModel->MaterialCount < AX_MODEL_MAX_MATERIALS; ++i) {
        cgltf_material* GltfMat = &Data->materials[i];

        // Create a PBR material description
        AxMaterialDesc MatDesc;
        memset(&MatDesc, 0, sizeof(MatDesc));
        MatDesc.Type = AX_MATERIAL_TYPE_PBR;

        // Copy name
        if (GltfMat->name) {
            strncpy(MatDesc.Name, GltfMat->name, sizeof(MatDesc.Name) - 1);
        } else {
            snprintf(MatDesc.Name, sizeof(MatDesc.Name), "Material_%zu", i);
        }

        // Initialize PBR defaults
        MatDesc.PBR.BaseColorFactor.X = 1.0f;
        MatDesc.PBR.BaseColorFactor.Y = 1.0f;
        MatDesc.PBR.BaseColorFactor.Z = 1.0f;
        MatDesc.PBR.BaseColorFactor.W = 1.0f;
        MatDesc.PBR.MetallicFactor = 1.0f;
        MatDesc.PBR.RoughnessFactor = 1.0f;
        MatDesc.PBR.AlphaCutoff = 0.5f;
        MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_OPAQUE;
        MatDesc.PBR.DoubleSided = GltfMat->double_sided;

        // Set texture indices to -1 (no texture)
        MatDesc.PBR.BaseColorTexture = -1;
        MatDesc.PBR.MetallicRoughnessTexture = -1;
        MatDesc.PBR.NormalTexture = -1;
        MatDesc.PBR.EmissiveTexture = -1;
        MatDesc.PBR.OcclusionTexture = -1;

        // Copy PBR metallic roughness properties and load textures
        if (GltfMat->has_pbr_metallic_roughness) {
            cgltf_pbr_metallic_roughness* PBR = &GltfMat->pbr_metallic_roughness;
            MatDesc.PBR.BaseColorFactor.X = PBR->base_color_factor[0];
            MatDesc.PBR.BaseColorFactor.Y = PBR->base_color_factor[1];
            MatDesc.PBR.BaseColorFactor.Z = PBR->base_color_factor[2];
            MatDesc.PBR.BaseColorFactor.W = PBR->base_color_factor[3];
            MatDesc.PBR.MetallicFactor = PBR->metallic_factor;
            MatDesc.PBR.RoughnessFactor = PBR->roughness_factor;

            // Load base color texture
            MatDesc.PBR.BaseColorTexture = LoadGltfTexture(&PBR->base_color_texture);

            // Load metallic roughness texture
            MatDesc.PBR.MetallicRoughnessTexture = LoadGltfTexture(&PBR->metallic_roughness_texture);
        }

        // Copy emissive factor
        MatDesc.PBR.EmissiveFactor.X = GltfMat->emissive_factor[0];
        MatDesc.PBR.EmissiveFactor.Y = GltfMat->emissive_factor[1];
        MatDesc.PBR.EmissiveFactor.Z = GltfMat->emissive_factor[2];

        // Load normal texture
        MatDesc.PBR.NormalTexture = LoadGltfTexture(&GltfMat->normal_texture);

        // Load emissive texture
        MatDesc.PBR.EmissiveTexture = LoadGltfTexture(&GltfMat->emissive_texture);

        // Load occlusion texture
        MatDesc.PBR.OcclusionTexture = LoadGltfTexture(&GltfMat->occlusion_texture);

        // Set alpha mode
        switch (GltfMat->alpha_mode) {
            case cgltf_alpha_mode_opaque:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_OPAQUE;
                break;
            case cgltf_alpha_mode_mask:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_MASK;
                MatDesc.PBR.AlphaCutoff = GltfMat->alpha_cutoff;
                break;
            case cgltf_alpha_mode_blend:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_BLEND;
                break;
            default:
                MatDesc.PBR.AlphaMode = AX_ALPHA_MODE_OPAQUE;
                break;
        }

        // Debug output for material texture indices
        AX_LOG(DEBUG, "Material '%s' textures - Base:%d Normal:%d MetRough:%d Emissive:%d Occlusion:%d AlphaMode:%d",
               MatDesc.Name,
               MatDesc.PBR.BaseColorTexture,
               MatDesc.PBR.NormalTexture,
               MatDesc.PBR.MetallicRoughnessTexture,
               MatDesc.PBR.EmissiveTexture,
               MatDesc.PBR.OcclusionTexture,
               MatDesc.PBR.AlphaMode);

        // Create the material
        AxMaterialHandle MatHandle = CreateMaterial(&MatDesc);
        if (!AX_HANDLE_IS_VALID(MatHandle)) {
            AX_LOG(ERROR, "Failed to create material: %s", MatDesc.Name);
        } else {
            OutModel->Materials[OutModel->MaterialCount] = MatHandle;
            OutModel->MaterialCount++;
        }
    }

    // Compute node transforms with hierarchy (needed for proper mesh positioning)
    // This mirrors the logic from the original OpenGLGame.cpp
    if (Data->nodes_count > 0 && Data->nodes_count <= AX_MODEL_MAX_TRANSFORMS) {
        // Track which nodes have been processed
        bool* NodeProcessed = static_cast<bool*>(AxAlloc(m_Allocator, sizeof(bool) * Data->nodes_count));
        if (NodeProcessed) {
            memset(NodeProcessed, 0, sizeof(bool) * Data->nodes_count);

            // Helper function to find parent of a node
            auto FindParentIndex = [Data](size_t nodeIndex) -> size_t {
                for (size_t i = 0; i < Data->nodes_count; ++i) {
                    cgltf_node* potentialParent = &Data->nodes[i];
                    for (size_t childIdx = 0; childIdx < potentialParent->children_count; ++childIdx) {
                        if (potentialParent->children[childIdx] >= Data->nodes &&
                            potentialParent->children[childIdx] < Data->nodes + Data->nodes_count) {
                            size_t childNodeIndex = potentialParent->children[childIdx] - Data->nodes;
                            if (childNodeIndex == nodeIndex) {
                                return i;
                            }
                        }
                    }
                }
                return static_cast<size_t>(-1); // No parent found
            };

            // Recursive lambda to compute node transform
            std::function<AxMat4x4(size_t)> ComputeNodeTransform = [&](size_t nodeIndex) -> AxMat4x4 {
                if (nodeIndex >= Data->nodes_count) {
                    return Identity();
                }

                if (NodeProcessed[nodeIndex]) {
                    return OutModel->Transforms[nodeIndex];
                }

                cgltf_node* node = &Data->nodes[nodeIndex];
                AxMat4x4 localTransform;

                // Build local transform
                if (node->has_matrix) {
                    memcpy(&localTransform, node->matrix, sizeof(AxMat4x4));
                } else {
                    // Build from TRS
                    AxVec3 translation = node->has_translation ?
                        (AxVec3){node->translation[0], node->translation[1], node->translation[2]} :
                        (AxVec3){0.0f, 0.0f, 0.0f};

                    AxQuat rotation = node->has_rotation ?
                        (AxQuat){node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]} :
                        QuatIdentity();

                    AxVec3 scale = node->has_scale ?
                        (AxVec3){node->scale[0], node->scale[1], node->scale[2]} :
                        Vec3One();

                    // R * S + direct translation
                    AxMat4x4 R = QuatToMat4x4(rotation);
                    AxMat4x4 S = Identity();
                    S.E[0][0] = scale.X;
                    S.E[1][1] = scale.Y;
                    S.E[2][2] = scale.Z;

                    AxMat4x4 RS = Mat4x4Mul(R, S);
                    localTransform = RS;

                    // Apply translation directly
                    localTransform.E[3][0] = translation.X;
                    localTransform.E[3][1] = translation.Y;
                    localTransform.E[3][2] = translation.Z;
                    localTransform.E[3][3] = 1.0f;
                }

                // Find parent and multiply: ParentMatrix * LocalMatrix
                AxMat4x4 finalTransform = localTransform;
                size_t parentIndex = FindParentIndex(nodeIndex);

                if (parentIndex != static_cast<size_t>(-1) && parentIndex != nodeIndex) {
                    AxMat4x4 parentTransform = ComputeNodeTransform(parentIndex);
                    finalTransform = Mat4x4Mul(parentTransform, localTransform);
                }

                OutModel->Transforms[nodeIndex] = finalTransform;
                NodeProcessed[nodeIndex] = true;
                return finalTransform;
            };

            // Process all nodes to build final transforms with hierarchy
            for (size_t nodeIndex = 0; nodeIndex < Data->nodes_count; ++nodeIndex) {
                ComputeNodeTransform(nodeIndex);
            }

            OutModel->TransformCount = static_cast<uint32_t>(Data->nodes_count);
            AxFree(m_Allocator, NodeProcessed);
        }
    }

    // Load all meshes from the GLTF file
    for (size_t meshIdx = 0; meshIdx < Data->meshes_count && OutModel->MeshCount < AX_MODEL_MAX_MESHES; ++meshIdx) {
        cgltf_mesh* GltfMesh = &Data->meshes[meshIdx];

        for (size_t primIdx = 0; primIdx < GltfMesh->primitives_count && OutModel->MeshCount < AX_MODEL_MAX_MESHES; ++primIdx) {
            cgltf_primitive* Primitive = &GltfMesh->primitives[primIdx];

            // Find position accessor (required)
            cgltf_accessor* PositionAccessor = nullptr;
            cgltf_accessor* NormalAccessor = nullptr;
            cgltf_accessor* TexCoordAccessor = nullptr;
            cgltf_accessor* TangentAccessor = nullptr;

            for (size_t attrIdx = 0; attrIdx < Primitive->attributes_count; ++attrIdx) {
                cgltf_attribute* Attr = &Primitive->attributes[attrIdx];
                switch (Attr->type) {
                    case cgltf_attribute_type_position:
                        PositionAccessor = Attr->data;
                        break;
                    case cgltf_attribute_type_normal:
                        NormalAccessor = Attr->data;
                        break;
                    case cgltf_attribute_type_texcoord:
                        if (Attr->index == 0) {
                            TexCoordAccessor = Attr->data;
                        }
                        break;
                    case cgltf_attribute_type_tangent:
                        TangentAccessor = Attr->data;
                        break;
                    default:
                        break;
                }
            }

            if (!PositionAccessor) {
                continue; // Skip primitives without position data
            }

            // Create vertex array
            uint32_t VertexCount = static_cast<uint32_t>(PositionAccessor->count);
            AxVertex* Vertices = static_cast<AxVertex*>(AxAlloc(m_Allocator, sizeof(AxVertex) * VertexCount));
            if (!Vertices) {
                continue;
            }

            memset(Vertices, 0, sizeof(AxVertex) * VertexCount);

            // Read positions
            for (uint32_t i = 0; i < VertexCount; ++i) {
                cgltf_accessor_read_float(PositionAccessor, i, &Vertices[i].Position.X, 3);
            }

            // Read normals
            if (NormalAccessor) {
                for (uint32_t i = 0; i < VertexCount; ++i) {
                    cgltf_accessor_read_float(NormalAccessor, i, &Vertices[i].Normal.X, 3);
                }
            }

            // Read texture coordinates
            if (TexCoordAccessor) {
                for (uint32_t i = 0; i < VertexCount; ++i) {
                    cgltf_accessor_read_float(TexCoordAccessor, i, &Vertices[i].TexCoord.X, 2);
                }
            }

            // Read tangents or generate defaults
            if (TangentAccessor) {
                for (uint32_t i = 0; i < VertexCount; ++i) {
                    cgltf_accessor_read_float(TangentAccessor, i, &Vertices[i].Tangent.X, 4);
                }
            } else if (NormalAccessor) {
                // Generate default tangents when not provided by the model
                // This allows normal mapping to work even without explicit tangents
                for (uint32_t i = 0; i < VertexCount; ++i) {
                    // Choose a tangent perpendicular to the normal
                    AxVec3 n = Vertices[i].Normal;
                    AxVec3 t;
                    if (fabsf(n.X) < 0.9f) {
                        t = (AxVec3){ 1.0f, 0.0f, 0.0f };
                    } else {
                        t = (AxVec3){ 0.0f, 1.0f, 0.0f };
                    }
                    // Gram-Schmidt orthogonalize
                    float dot = n.X * t.X + n.Y * t.Y + n.Z * t.Z;
                    t.X -= n.X * dot;
                    t.Y -= n.Y * dot;
                    t.Z -= n.Z * dot;
                    // Normalize
                    float len = sqrtf(t.X * t.X + t.Y * t.Y + t.Z * t.Z);
                    if (len > 0.0001f) {
                        t.X /= len;
                        t.Y /= len;
                        t.Z /= len;
                    }
                    Vertices[i].Tangent.X = t.X;
                    Vertices[i].Tangent.Y = t.Y;
                    Vertices[i].Tangent.Z = t.Z;
                    Vertices[i].Tangent.W = 1.0f; // Positive handedness
                }
            }

            // Read indices
            uint32_t IndexCount = 0;
            uint32_t* Indices = nullptr;

            if (Primitive->indices) {
                IndexCount = static_cast<uint32_t>(Primitive->indices->count);
                Indices = static_cast<uint32_t*>(AxAlloc(m_Allocator, sizeof(uint32_t) * IndexCount));
                if (Indices) {
                    for (uint32_t i = 0; i < IndexCount; ++i) {
                        Indices[i] = static_cast<uint32_t>(cgltf_accessor_read_index(Primitive->indices, i));
                    }
                }
            }

            // Allocate mesh slot
            AxMeshHandle MeshHandle = m_Meshes.Allocate();
            if (!AX_HANDLE_IS_VALID(MeshHandle)) {
                AxFree(m_Allocator, Vertices);
                if (Indices) {
                    AxFree(m_Allocator, Indices);
                }
                continue;
            }

            ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlot(MeshHandle);
            if (!Slot) {
                AxFree(m_Allocator, Vertices);
                if (Indices) {
                    AxFree(m_Allocator, Indices);
                }
                continue;
            }

            // Allocate mesh data
            AxMesh* Mesh = static_cast<AxMesh*>(AxAlloc(m_Allocator, sizeof(AxMesh)));
            if (!Mesh) {
                m_Meshes.Free(MeshHandle.Index);
                AxFree(m_Allocator, Vertices);
                if (Indices) {
                    AxFree(m_Allocator, Indices);
                }
                continue;
            }

            memset(Mesh, 0, sizeof(AxMesh));
            Mesh->IndexCount = IndexCount;

            // Copy mesh name
            if (GltfMesh->name) {
                snprintf(Mesh->Name, sizeof(Mesh->Name), "%s_%zu", GltfMesh->name, primIdx);
            } else {
                snprintf(Mesh->Name, sizeof(Mesh->Name), "Mesh_%zu_%zu", meshIdx, primIdx);
            }

            // Upload to GPU
            if (m_RenderAPI) {
                m_RenderAPI->InitMesh(Mesh, Vertices, Indices, VertexCount, IndexCount);
            }

            // Free temporary data
            AxFree(m_Allocator, Vertices);
            if (Indices) {
                AxFree(m_Allocator, Indices);
            }

            // Initialize mesh indices to defaults
            Mesh->MaterialIndex = 0;
            Mesh->BaseColorTexture = 0;
            Mesh->NormalTexture = 0;
            Mesh->TransformIndex = 0;

            // Map mesh to material
            if (Primitive->material) {
                size_t MaterialIndex = Primitive->material - Data->materials;
                if (MaterialIndex < OutModel->MaterialCount) {
                    Mesh->MaterialIndex = static_cast<uint32_t>(MaterialIndex);
                    OutModel->MeshMaterials[OutModel->MeshCount] = static_cast<int32_t>(MaterialIndex);

                    // Get material to find texture indices
                    const ResourceSlot<AxMaterialDesc>* MatSlot = GetMaterialSlot(OutModel->Materials[MaterialIndex]);
                    const AxMaterialDesc* MatDesc = MatSlot ? MatSlot->Data : nullptr;
                    if (MatDesc && MatDesc->Type == AX_MATERIAL_TYPE_PBR) {
                        if (MatDesc->PBR.BaseColorTexture >= 0) {
                            Mesh->BaseColorTexture = static_cast<uint32_t>(MatDesc->PBR.BaseColorTexture);
                        }
                        if (MatDesc->PBR.NormalTexture >= 0) {
                            Mesh->NormalTexture = static_cast<uint32_t>(MatDesc->PBR.NormalTexture);
                        }
                    }
                }
            }

            // Find which node references this mesh and store the transform index
            for (size_t nodeIndex = 0; nodeIndex < Data->nodes_count; ++nodeIndex) {
                cgltf_node* node = &Data->nodes[nodeIndex];
                if (node->mesh && node->mesh == GltfMesh) {
                    Mesh->TransformIndex = static_cast<uint32_t>(nodeIndex);
                    break;
                }
            }

            // Store mesh in model
            OutModel->MeshTransformIndices[OutModel->MeshCount] = Mesh->TransformIndex;
            Slot->Data = Mesh;
            OutModel->Meshes[OutModel->MeshCount] = MeshHandle;
            OutModel->MeshCount++;
        }
    }

    cgltf_free(Data);

    // Print texture cache statistics for this model load
    AX_LOG_IF(DEBUG, ImageCacheHits > 0 || ImageCacheMisses > 0,
              "Texture cache stats - %u hits, %u misses (%.1f%% hit rate)",
              ImageCacheHits, ImageCacheMisses,
              (ImageCacheHits + ImageCacheMisses) > 0 ?
              100.0f * ImageCacheHits / (ImageCacheHits + ImageCacheMisses) : 0.0f);

    AX_LOG(INFO, "Loaded model %s - %u meshes, %u textures, %u materials, %u transforms",
           OutModel->Name, OutModel->MeshCount, OutModel->TextureCount, OutModel->MaterialCount, OutModel->TransformCount);

    return (OutModel->MeshCount > 0);
}

AxModelHandle ResourceSystem::LoadModel(std::string_view Path)
{
    if (!m_Initialized) {
        AX_LOG(ERROR, "Not initialized");
        return AX_INVALID_HANDLE;
    }

    if (Path.empty()) {
        AX_LOG(ERROR, "Invalid model path");
        return AX_INVALID_HANDLE;
    }

    // Allocate a slot for the model
    AxModelHandle Handle = m_Models.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        AX_LOG(ERROR, "Failed to allocate model slot");
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxModelData>* Slot = m_Models.GetSlot(Handle);
    if (!Slot) {
        AX_LOG(ERROR, "Failed to get model slot");
        return AX_INVALID_HANDLE;
    }

    // Allocate model data structure
    AxModelData* Model = static_cast<AxModelData*>(AxAlloc(m_Allocator, sizeof(AxModelData)));
    if (!Model) {
        AX_LOG(ERROR, "Failed to allocate model data");
        m_Models.Free(Handle.Index);
        return AX_INVALID_HANDLE;
    }

    // Load the model data
    if (!LoadModelInternal(Path, Model)) {
        AX_LOG(ERROR, "Failed to load model: %.*s", static_cast<int>(Path.size()), Path.data());
        AxFree(m_Allocator, Model);
        m_Models.Free(Handle.Index);
        return AX_INVALID_HANDLE;
    }

    // Store model in slot
    Slot->Data = Model;

    AX_LOG(INFO, "Created model handle [%u:%u] - %s",
           Handle.Index, Handle.Generation, Model->Name);

    return Handle;
}

AxModelHandle ResourceSystem::CreateModelSlot()
{
    return m_Models.Allocate();
}

AxMeshHandle ResourceSystem::CreateMeshFromData(struct AxVertex* Vertices, uint32_t* Indices,
                                                 uint32_t VertexCount, uint32_t IndexCount,
                                                 const char* Name)
{
    if (!Vertices || !Indices || VertexCount == 0 || IndexCount == 0) {
        return AX_INVALID_HANDLE;
    }

    AxMeshHandle Handle = m_Meshes.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlot(Handle);
    if (!Slot) {
        return AX_INVALID_HANDLE;
    }

    AxMesh* Mesh = static_cast<AxMesh*>(AxAlloc(m_Allocator, sizeof(AxMesh)));
    if (!Mesh) {
        m_Meshes.Free(Handle.Index);
        return AX_INVALID_HANDLE;
    }

    memset(Mesh, 0, sizeof(AxMesh));
    if (Name) {
        strncpy(Mesh->Name, Name, sizeof(Mesh->Name) - 1);
    }

    // Upload to GPU
    if (m_RenderAPI) {
        m_RenderAPI->InitMesh(Mesh, Vertices, Indices, VertexCount, IndexCount);
    }

    Mesh->TransformIndex = 0;
    Mesh->MaterialIndex = UINT32_MAX;
    Mesh->BaseColorTexture = UINT32_MAX;
    Mesh->NormalTexture = UINT32_MAX;

    Slot->Data = Mesh;
    return Handle;
}

AxModelHandle ResourceSystem::CreateModelFromMesh(AxMeshHandle MeshHandle,
                                                   const char* Name, const char* Path)
{
    if (!AX_HANDLE_IS_VALID(MeshHandle)) {
        return AX_INVALID_HANDLE;
    }

    AxModelHandle Handle = m_Models.Allocate();
    if (!AX_HANDLE_IS_VALID(Handle)) {
        return AX_INVALID_HANDLE;
    }

    ResourceSlot<AxModelData>* Slot = m_Models.GetSlot(Handle);
    if (!Slot) {
        return AX_INVALID_HANDLE;
    }

    AxModelData* Model = static_cast<AxModelData*>(AxAlloc(m_Allocator, sizeof(AxModelData)));
    if (!Model) {
        m_Models.Free(Handle.Index);
        return AX_INVALID_HANDLE;
    }

    memset(Model, 0, sizeof(AxModelData));
    if (Name) {
        strncpy(Model->Name, Name, sizeof(Model->Name) - 1);
    }
    if (Path) {
        strncpy(Model->BasePath, Path, sizeof(Model->BasePath) - 1);
    }

    Model->MeshCount = 1;
    Model->Meshes[0] = MeshHandle;
    Model->TextureCount = 0;
    Model->MaterialCount = 0;
    Model->TransformCount = 1;
    Model->Transforms[0] = Identity();
    Model->MeshMaterials[0] = -1;
    Model->MeshTransformIndices[0] = 0;

    Slot->Data = Model;
    return Handle;
}

ResourceSlot<AxModelData>* ResourceSystem::GetModelSlot(AxModelHandle Handle)
{
    return m_Models.GetSlot(Handle);
}

const ResourceSlot<AxModelData>* ResourceSystem::GetModelSlot(AxModelHandle Handle) const
{
    return m_Models.GetSlot(Handle);
}

const struct AxModelData* ResourceSystem::GetModel(AxModelHandle Handle) const
{
    const ResourceSlot<AxModelData>* Slot = m_Models.GetSlot(Handle);
    return Slot ? Slot->Data : nullptr;
}

bool ResourceSystem::IsModelValid(AxModelHandle Handle) const
{
    return m_Models.IsValid(Handle);
}

uint32_t ResourceSystem::GetModelCount() const
{
    return m_Models.GetUsedCount();
}

AxModelHandle ResourceSystem::GetModelByIndex(uint32_t Index) const
{
    // Iterate through slots to find the Nth in-use slot
    uint32_t Count = 0;
    for (uint32_t i = 1; i < m_Models.GetCapacity(); ++i) {
        const ResourceSlot<AxModelData>* Slot = m_Models.GetSlotByIndex(i);
        if (Slot && Slot->InUse) {
            if (Count == Index) {
                AxModelHandle Handle;
                Handle.Index = i;
                Handle.Generation = Slot->Generation;
                return Handle;
            }
            Count++;
        }
    }
    return AX_INVALID_HANDLE;
}

AxModelHandle ResourceSystem::AcquireModel(AxModelHandle Handle)
{
    ResourceSlot<AxModelData>* Slot = m_Models.GetSlot(Handle);
    if (!Slot || !Slot->Data) {
        return AX_INVALID_HANDLE;
    }

    // Increment model refcount
    Slot->RefCount++;

    // Also acquire references to all child resources
    AxModelData* Model = Slot->Data;

    for (uint32_t i = 0; i < Model->TextureCount; ++i) {
        if (AX_HANDLE_IS_VALID(Model->Textures[i])) {
            AcquireTexture(Model->Textures[i]);
        }
    }

    for (uint32_t i = 0; i < Model->MeshCount; ++i) {
        if (AX_HANDLE_IS_VALID(Model->Meshes[i])) {
            AcquireMesh(Model->Meshes[i]);
        }
    }

    for (uint32_t i = 0; i < Model->MaterialCount; ++i) {
        if (AX_HANDLE_IS_VALID(Model->Materials[i])) {
            AcquireMaterial(Model->Materials[i]);
        }
    }

    return Handle;
}

void ResourceSystem::ReleaseModel(AxModelHandle Handle)
{
    ResourceSlot<AxModelData>* Slot = m_Models.GetSlot(Handle);
    if (!Slot || Slot->RefCount == 0) {
        return;
    }

    // Release references to all child resources first
    if (Slot->Data) {
        AxModelData* Model = Slot->Data;

        for (uint32_t i = 0; i < Model->TextureCount; ++i) {
            if (AX_HANDLE_IS_VALID(Model->Textures[i])) {
                ReleaseTexture(Model->Textures[i]);
            }
        }

        for (uint32_t i = 0; i < Model->MeshCount; ++i) {
            if (AX_HANDLE_IS_VALID(Model->Meshes[i])) {
                ReleaseMesh(Model->Meshes[i]);
            }
        }

        for (uint32_t i = 0; i < Model->MaterialCount; ++i) {
            if (AX_HANDLE_IS_VALID(Model->Materials[i])) {
                ReleaseMaterial(Model->Materials[i]);
            }
        }
    }

    // Decrement model refcount
    Slot->RefCount--;
    if (Slot->RefCount == 0) {
        // Queue for deferred deletion
        QueueForDeletion(ResourceType::Model, Handle.Index, Handle.Generation);
    }
}

uint32_t ResourceSystem::GetModelRefCount(AxModelHandle Handle) const
{
    const ResourceSlot<AxModelData>* Slot = m_Models.GetSlot(Handle);
    return Slot ? Slot->RefCount : 0;
}

//=============================================================================
// Deferred Destruction
//=============================================================================

void ResourceSystem::QueueForDeletion(ResourceType Type, uint32_t SlotIndex, uint32_t Generation)
{
    // Grow queue if needed
    if (m_PendingCount >= m_PendingCapacity) {
        uint32_t NewCapacity = m_PendingCapacity * 2;
        size_t OldSize = sizeof(PendingDeletion) * m_PendingCapacity;
        size_t NewSize = sizeof(PendingDeletion) * NewCapacity;
        PendingDeletion* NewQueue = static_cast<PendingDeletion*>(
            AxRealloc(m_Allocator, m_PendingDeletions, OldSize, NewSize));
        if (!NewQueue) {
            // Failed to grow queue - process immediately instead
            PendingDeletion Immediate;
            Immediate.Type = Type;
            Immediate.SlotIndex = SlotIndex;
            Immediate.Generation = Generation;
            Immediate.FrameQueued = 0;
            ProcessDeletion(Immediate);
            return;
        }
        m_PendingDeletions = NewQueue;
        m_PendingCapacity = NewCapacity;
    }

    // Add to queue
    m_PendingDeletions[m_PendingCount].Type = Type;
    m_PendingDeletions[m_PendingCount].SlotIndex = SlotIndex;
    m_PendingDeletions[m_PendingCount].Generation = Generation;
    m_PendingDeletions[m_PendingCount].FrameQueued = 0; // Could track frame number here
    m_PendingCount++;
}

void ResourceSystem::ProcessPendingReleases()
{
    ProcessPendingReleasesLimited(m_PendingCount);
}

void ResourceSystem::ProcessPendingReleasesLimited(uint32_t MaxCount)
{
    uint32_t ToProcess = (MaxCount < m_PendingCount) ? MaxCount : m_PendingCount;

    for (uint32_t i = 0; i < ToProcess; ++i) {
        ProcessDeletion(m_PendingDeletions[i]);
    }

    // Remove processed items (shift remaining)
    if (ToProcess > 0 && ToProcess < m_PendingCount) {
        memmove(m_PendingDeletions,
                m_PendingDeletions + ToProcess,
                sizeof(PendingDeletion) * (m_PendingCount - ToProcess));
    }
    m_PendingCount -= ToProcess;
}

uint32_t ResourceSystem::GetPendingReleaseCount() const
{
    return m_PendingCount;
}

void ResourceSystem::ProcessDeletion(const PendingDeletion& Deletion)
{
    switch (Deletion.Type) {
        case ResourceType::Texture: {
            ResourceSlot<AxTexture>* Slot = m_Textures.GetSlotByIndex(Deletion.SlotIndex);
            if (Slot && Slot->InUse && Slot->Generation == Deletion.Generation) {
                // Remove from path cache
                AxTextureHandle HandleToRemove = { Deletion.SlotIndex, Deletion.Generation };
                TexturePathCacheRemove(HandleToRemove);

                // Destroy GPU resource
                if (Slot->Data && m_RenderAPI) {
                    m_RenderAPI->DestroyTexture(Slot->Data);
                }
                // Free CPU memory
                if (Slot->Data) {
                    AxFree(m_Allocator, Slot->Data);
                }
                // Free slot
                m_Textures.Free(Deletion.SlotIndex);
            }
            break;
        }
        case ResourceType::Mesh: {
            ResourceSlot<AxMesh>* Slot = m_Meshes.GetSlotByIndex(Deletion.SlotIndex);
            if (Slot && Slot->InUse && Slot->Generation == Deletion.Generation) {
                // GPU mesh cleanup would go here if we had DestroyMesh
                // Free CPU memory
                if (Slot->Data) {
                    AxFree(m_Allocator, Slot->Data);
                }
                // Free slot
                m_Meshes.Free(Deletion.SlotIndex);
            }
            break;
        }
        case ResourceType::Shader: {
            ResourceSlot<AxShaderData>* Slot = m_Shaders.GetSlotByIndex(Deletion.SlotIndex);
            if (Slot && Slot->InUse && Slot->Generation == Deletion.Generation) {
                // Destroy GPU program
                if (Slot->Data && m_RenderAPI) {
                    m_RenderAPI->DestroyProgram(Slot->Data->ShaderHandle);
                }
                // Free CPU memory
                if (Slot->Data) {
                    AxFree(m_Allocator, Slot->Data);
                }
                // Free slot
                m_Shaders.Free(Deletion.SlotIndex);
            }
            break;
        }
        case ResourceType::Material: {
            ResourceSlot<AxMaterialDesc>* Slot = m_Materials.GetSlotByIndex(Deletion.SlotIndex);
            if (Slot && Slot->InUse && Slot->Generation == Deletion.Generation) {
                // Free custom data if present
                if (Slot->Data &&
                    Slot->Data->Type == AX_MATERIAL_TYPE_CUSTOM &&
                    Slot->Data->Custom.CustomData != nullptr) {
                    AxFree(m_Allocator, Slot->Data->Custom.CustomData);
                }
                // Free CPU memory
                if (Slot->Data) {
                    AxFree(m_Allocator, Slot->Data);
                }
                // Free slot
                m_Materials.Free(Deletion.SlotIndex);
            }
            break;
        }
        case ResourceType::Model: {
            ResourceSlot<AxModelData>* Slot = m_Models.GetSlotByIndex(Deletion.SlotIndex);
            if (Slot && Slot->InUse && Slot->Generation == Deletion.Generation) {
                // Note: Child resources (textures, meshes, materials) are already
                // released by ReleaseModel() before queuing for deletion.
                // We just need to free the model data structure itself.
                if (Slot->Data) {
                    AxFree(m_Allocator, Slot->Data);
                }
                // Free slot
                m_Models.Free(Deletion.SlotIndex);
            }
            break;
        }
        default:
            break;
    }
}

//=============================================================================
// Statistics
//=============================================================================

void ResourceSystem::GetStats(struct AxResourceStats* OutStats) const
{
    if (!OutStats) {
        return;
    }

    OutStats->TexturesLoaded = m_Textures.GetUsedCount();
    OutStats->MeshesLoaded = m_Meshes.GetUsedCount();
    OutStats->ShadersLoaded = m_Shaders.GetUsedCount();
    OutStats->MaterialsLoaded = m_Materials.GetUsedCount();
    OutStats->ModelsLoaded = m_Models.GetUsedCount();
    OutStats->PendingDeletions = m_PendingCount;

    // Calculate memory usage from allocator
    OutStats->TotalMemoryUsed = m_Allocator ? m_Allocator->BytesAllocated : 0;
}
