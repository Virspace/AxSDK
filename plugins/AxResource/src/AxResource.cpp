/**
 * AxResource.cpp - Resource API C Interface Implementation
 *
 * This file provides the C API wrapper functions that delegate to the
 * internal ResourceSystem class. It also contains the plugin entry points.
 *
 * Phase 3: Wired up actual resource loading functions (LoadTexture, LoadMesh,
 * LoadShader, CreateMaterial) to ResourceSystem implementations.
 *
 * Phase 4: Added LoadModel/ReleaseModel convenience API and statistics functions.
 *
 * Phase 6: Updated to handle-based model tracking with ModelManager.
 * - LoadModel now returns AxModelHandle instead of bool
 * - Added GetModel, IsModelValid, GetModelCount, GetModelByIndex
 * - Added AcquireModel/ReleaseModel for reference counting
 * - Added GetModelRefCount for debugging
 *
 */

#include "AxResource/AxResource.h"
#include "ResourceSystem.h"
#include "Foundation/AxPlugin.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxAllocatorAPI.h"
#include "AxOpenGL/AxOpenGL.h"
#include "AxOpenGL/AxOpenGLTypes.h"

#include <cstdlib>
#include <cstring>

//=============================================================================
// Forward Declarations
//=============================================================================

static void Initialize(struct AxAPIRegistry* Registry,
                       struct AxAllocator* Allocator,
                       const struct AxResourceInitOptions* Options);
static void Shutdown(void);
static bool IsInitialized(void);

// Texture
static AxTextureHandle LoadTexture(const char* Path,
                                    const struct AxTextureLoadOptions* Options);
static const struct AxTexture* GetTexture(AxTextureHandle Handle);
static bool GetTextureInfo(AxTextureHandle Handle, struct AxTextureInfo* OutInfo);
static bool IsTextureValid(AxTextureHandle Handle);
static AxTextureHandle AcquireTexture(AxTextureHandle Handle);
static void ReleaseTexture(AxTextureHandle Handle);

// Mesh
static AxMeshHandle LoadMesh(const char* Path,
                              const struct AxMeshLoadOptions* Options);
static const struct AxMesh* GetMesh(AxMeshHandle Handle);
static bool GetMeshInfo(AxMeshHandle Handle, struct AxMeshInfo* OutInfo);
static bool IsMeshValid(AxMeshHandle Handle);
static AxMeshHandle AcquireMesh(AxMeshHandle Handle);
static void ReleaseMesh(AxMeshHandle Handle);

// Shader
static AxShaderHandle LoadShader(const char* VertexPath,
                                  const char* FragmentPath,
                                  const struct AxShaderLoadOptions* Options);
static const struct AxShaderData* GetShader(AxShaderHandle Handle);
static uint32_t GetShaderProgram(AxShaderHandle Handle);
static bool GetShaderInfo(AxShaderHandle Handle, struct AxShaderInfo* OutInfo);
static bool IsShaderValid(AxShaderHandle Handle);
static AxShaderHandle AcquireShader(AxShaderHandle Handle);
static void ReleaseShader(AxShaderHandle Handle);

// Material
static AxMaterialHandle CreateMaterial(const struct AxMaterialDesc* Desc);
static const struct AxMaterialDesc* GetMaterial(AxMaterialHandle Handle);
static bool IsMaterialValid(AxMaterialHandle Handle);
static AxMaterialHandle AcquireMaterial(AxMaterialHandle Handle);
static void ReleaseMaterial(AxMaterialHandle Handle);

// Programmatic Resource Creation
static AxMeshHandle CreateMeshFromData(struct AxVertex* Vertices, uint32_t* Indices,
                                        uint32_t VertexCount, uint32_t IndexCount,
                                        const char* Name);
static AxModelHandle CreateModelFromMesh(AxMeshHandle MeshHandle,
                                          const char* Name, const char* Path);

// Model (Handle-based - Phase 6)
static AxModelHandle LoadModel(const char* Path);
static const struct AxModelData* GetModel(AxModelHandle Handle);
static bool IsModelValid(AxModelHandle Handle);
static uint32_t GetModelCount(void);
static AxModelHandle GetModelByIndex(uint32_t Index);
static AxModelHandle AcquireModel(AxModelHandle Handle);
static void ReleaseModel(AxModelHandle Handle);

// Deferred Destruction
static void ProcessPendingReleases(void);
static void ProcessPendingReleasesLimited(uint32_t MaxCount);
static uint32_t GetPendingReleaseCount(void);

// Statistics
static void GetStats(struct AxResourceStats* OutStats);
static uint32_t GetTextureRefCount(AxTextureHandle Handle);
static uint32_t GetMeshRefCount(AxMeshHandle Handle);
static uint32_t GetShaderRefCount(AxShaderHandle Handle);
static uint32_t GetMaterialRefCount(AxMaterialHandle Handle);
static uint32_t GetModelRefCount(AxModelHandle Handle);

//=============================================================================
// API Structure
//=============================================================================

static struct AxResourceAPI s_ResourceAPI = {
    // Lifecycle
    .Initialize = Initialize,
    .Shutdown = Shutdown,
    .IsInitialized = IsInitialized,

    // Texture
    .LoadTexture = LoadTexture,
    .GetTexture = GetTexture,
    .GetTextureInfo = GetTextureInfo,
    .IsTextureValid = IsTextureValid,
    .AcquireTexture = AcquireTexture,
    .ReleaseTexture = ReleaseTexture,

    // Mesh
    .LoadMesh = LoadMesh,
    .GetMesh = GetMesh,
    .GetMeshInfo = GetMeshInfo,
    .IsMeshValid = IsMeshValid,
    .AcquireMesh = AcquireMesh,
    .ReleaseMesh = ReleaseMesh,

    // Shader
    .LoadShader = LoadShader,
    .GetShader = GetShader,
    .GetShaderProgram = GetShaderProgram,
    .GetShaderInfo = GetShaderInfo,
    .IsShaderValid = IsShaderValid,
    .AcquireShader = AcquireShader,
    .ReleaseShader = ReleaseShader,

    // Material
    .CreateMaterial = CreateMaterial,
    .GetMaterial = GetMaterial,
    .IsMaterialValid = IsMaterialValid,
    .AcquireMaterial = AcquireMaterial,
    .ReleaseMaterial = ReleaseMaterial,

    // Model (Handle-based - Phase 6)
    .LoadModel = LoadModel,
    .GetModel = GetModel,
    .IsModelValid = IsModelValid,
    .GetModelCount = GetModelCount,
    .GetModelByIndex = GetModelByIndex,
    .AcquireModel = AcquireModel,
    .ReleaseModel = ReleaseModel,

    // Programmatic Resource Creation
    .CreateMeshFromData = CreateMeshFromData,
    .CreateModelFromMesh = CreateModelFromMesh,

    // Deferred Destruction
    .ProcessPendingReleases = ProcessPendingReleases,
    .ProcessPendingReleasesLimited = ProcessPendingReleasesLimited,
    .GetPendingReleaseCount = GetPendingReleaseCount,

    // Statistics
    .GetStats = GetStats,
    .GetTextureRefCount = GetTextureRefCount,
    .GetMeshRefCount = GetMeshRefCount,
    .GetShaderRefCount = GetShaderRefCount,
    .GetMaterialRefCount = GetMaterialRefCount,
    .GetModelRefCount = GetModelRefCount
};

struct AxResourceAPI* ResourceAPI = &s_ResourceAPI;

//=============================================================================
// Lifecycle Implementation
//=============================================================================

static void Initialize(struct AxAPIRegistry* Registry,
                       struct AxAllocator* Allocator,
                       const struct AxResourceInitOptions* Options)
{
    ResourceSystem::Get().Initialize(Registry, Allocator, Options);
}

static void Shutdown(void)
{
    ResourceSystem::Get().Shutdown();
}

static bool IsInitialized(void)
{
    return ResourceSystem::Get().IsInitialized();
}

//=============================================================================
// Texture Implementation
//=============================================================================

static AxTextureHandle LoadTexture(const char* Path,
                                    const struct AxTextureLoadOptions* Options)
{
    if (!Path) { return AX_INVALID_HANDLE; }
    return ResourceSystem::Get().LoadTexture(Path, Options);
}

static const struct AxTexture* GetTexture(AxTextureHandle Handle)
{
    const ResourceSlot<AxTexture>* Slot = ResourceSystem::Get().GetTextureSlot(Handle);
    return Slot ? Slot->Data : nullptr;
}

static bool GetTextureInfo(AxTextureHandle Handle, struct AxTextureInfo* OutInfo)
{
    if (!OutInfo) {
        return false;
    }

    const ResourceSlot<AxTexture>* Slot = ResourceSystem::Get().GetTextureSlot(Handle);
    if (!Slot || !Slot->Data) {
        return false;
    }

    OutInfo->Width = Slot->Data->Width;
    OutInfo->Height = Slot->Data->Height;
    OutInfo->Channels = Slot->Data->Channels;
    OutInfo->MipLevels = 1; // TODO: Track mip levels
    OutInfo->GPUHandle = Slot->Data->ID;
    OutInfo->HasAlpha = Slot->Data->Channels == 4;
    return true;
}

static bool IsTextureValid(AxTextureHandle Handle)
{
    return ResourceSystem::Get().IsTextureValid(Handle);
}

static AxTextureHandle AcquireTexture(AxTextureHandle Handle)
{
    return ResourceSystem::Get().AcquireTexture(Handle);
}

static void ReleaseTexture(AxTextureHandle Handle)
{
    ResourceSystem::Get().ReleaseTexture(Handle);
}

//=============================================================================
// Mesh Implementation
//=============================================================================

static AxMeshHandle LoadMesh(const char* Path,
                              const struct AxMeshLoadOptions* Options)
{
    if (!Path) { return AX_INVALID_HANDLE; }
    return ResourceSystem::Get().LoadMesh(Path, Options);
}

static const struct AxMesh* GetMesh(AxMeshHandle Handle)
{
    const ResourceSlot<AxMesh>* Slot = ResourceSystem::Get().GetMeshSlot(Handle);
    return Slot ? Slot->Data : nullptr;
}

static bool GetMeshInfo(AxMeshHandle Handle, struct AxMeshInfo* OutInfo)
{
    if (!OutInfo) {
        return false;
    }

    const ResourceSlot<AxMesh>* Slot = ResourceSystem::Get().GetMeshSlot(Handle);
    if (!Slot || !Slot->Data) {
        return false;
    }

    OutInfo->VertexCount = 0; // TODO: Track vertex count in AxMesh
    OutInfo->IndexCount = Slot->Data->IndexCount;
    OutInfo->GPUVertexBuffer = Slot->Data->VBO;
    OutInfo->GPUIndexBuffer = Slot->Data->EBO;
    OutInfo->GPUVertexArray = Slot->Data->VAO;
    return true;
}

static bool IsMeshValid(AxMeshHandle Handle)
{
    return ResourceSystem::Get().IsMeshValid(Handle);
}

static AxMeshHandle AcquireMesh(AxMeshHandle Handle)
{
    return ResourceSystem::Get().AcquireMesh(Handle);
}

static void ReleaseMesh(AxMeshHandle Handle)
{
    ResourceSystem::Get().ReleaseMesh(Handle);
}

//=============================================================================
// Shader Implementation
//=============================================================================

static AxShaderHandle LoadShader(const char* VertexPath,
                                  const char* FragmentPath,
                                  const struct AxShaderLoadOptions* Options)
{
    if (!VertexPath || !FragmentPath) { return AX_INVALID_HANDLE; }
    return ResourceSystem::Get().LoadShader(VertexPath, FragmentPath, Options);
}

static const struct AxShaderData* GetShader(AxShaderHandle Handle)
{
    const ResourceSlot<AxShaderData>* Slot = ResourceSystem::Get().GetShaderSlot(Handle);
    return Slot ? Slot->Data : nullptr;
}

static uint32_t GetShaderProgram(AxShaderHandle Handle)
{
    const ResourceSlot<AxShaderData>* Slot = ResourceSystem::Get().GetShaderSlot(Handle);
    return (Slot && Slot->Data) ? Slot->Data->ShaderHandle : 0;
}

static bool GetShaderInfo(AxShaderHandle Handle, struct AxShaderInfo* OutInfo)
{
    if (!OutInfo) {
        return false;
    }

    const ResourceSlot<AxShaderData>* Slot = ResourceSystem::Get().GetShaderSlot(Handle);
    if (!Slot || !Slot->Data) {
        return false;
    }

    OutInfo->ProgramID = Slot->Data->ShaderHandle;
    OutInfo->VertexPath = nullptr; // TODO: Store paths
    OutInfo->FragmentPath = nullptr;
    return true;
}

static bool IsShaderValid(AxShaderHandle Handle)
{
    return ResourceSystem::Get().IsShaderValid(Handle);
}

static AxShaderHandle AcquireShader(AxShaderHandle Handle)
{
    return ResourceSystem::Get().AcquireShader(Handle);
}

static void ReleaseShader(AxShaderHandle Handle)
{
    ResourceSystem::Get().ReleaseShader(Handle);
}

//=============================================================================
// Material Implementation
//=============================================================================

static AxMaterialHandle CreateMaterial(const struct AxMaterialDesc* Desc)
{
    return ResourceSystem::Get().CreateMaterial(Desc);
}

static const struct AxMaterialDesc* GetMaterial(AxMaterialHandle Handle)
{
    const ResourceSlot<AxMaterialDesc>* Slot = ResourceSystem::Get().GetMaterialSlot(Handle);
    return Slot ? Slot->Data : nullptr;
}

static bool IsMaterialValid(AxMaterialHandle Handle)
{
    return ResourceSystem::Get().IsMaterialValid(Handle);
}

static AxMaterialHandle AcquireMaterial(AxMaterialHandle Handle)
{
    return ResourceSystem::Get().AcquireMaterial(Handle);
}

static void ReleaseMaterial(AxMaterialHandle Handle)
{
    ResourceSystem::Get().ReleaseMaterial(Handle);
}

//=============================================================================
// Programmatic Resource Creation Implementation
//=============================================================================

static AxMeshHandle CreateMeshFromData(struct AxVertex* Vertices, uint32_t* Indices,
                                        uint32_t VertexCount, uint32_t IndexCount,
                                        const char* Name)
{
    return ResourceSystem::Get().CreateMeshFromData(Vertices, Indices,
                                                     VertexCount, IndexCount, Name);
}

static AxModelHandle CreateModelFromMesh(AxMeshHandle MeshHandle,
                                          const char* Name, const char* Path)
{
    return ResourceSystem::Get().CreateModelFromMesh(MeshHandle, Name, Path);
}

//=============================================================================
// Model Implementation (Handle-based - Phase 6)
//=============================================================================

static AxModelHandle LoadModel(const char* Path)
{
    if (!Path) { return AX_INVALID_HANDLE; }
    return ResourceSystem::Get().LoadModel(Path);
}

static const struct AxModelData* GetModel(AxModelHandle Handle)
{
    return ResourceSystem::Get().GetModel(Handle);
}

static bool IsModelValid(AxModelHandle Handle)
{
    return ResourceSystem::Get().IsModelValid(Handle);
}

static uint32_t GetModelCount(void)
{
    return ResourceSystem::Get().GetModelCount();
}

static AxModelHandle GetModelByIndex(uint32_t Index)
{
    return ResourceSystem::Get().GetModelByIndex(Index);
}

static AxModelHandle AcquireModel(AxModelHandle Handle)
{
    return ResourceSystem::Get().AcquireModel(Handle);
}

static void ReleaseModel(AxModelHandle Handle)
{
    ResourceSystem::Get().ReleaseModel(Handle);
}

//=============================================================================
// Deferred Destruction Implementation
//=============================================================================

static void ProcessPendingReleases(void)
{
    ResourceSystem::Get().ProcessPendingReleases();
}

static void ProcessPendingReleasesLimited(uint32_t MaxCount)
{
    ResourceSystem::Get().ProcessPendingReleasesLimited(MaxCount);
}

static uint32_t GetPendingReleaseCount(void)
{
    return ResourceSystem::Get().GetPendingReleaseCount();
}

//=============================================================================
// Statistics Implementation
//=============================================================================

static void GetStats(struct AxResourceStats* OutStats)
{
    ResourceSystem::Get().GetStats(OutStats);
}

static uint32_t GetTextureRefCount(AxTextureHandle Handle)
{
    return ResourceSystem::Get().GetTextureRefCount(Handle);
}

static uint32_t GetMeshRefCount(AxMeshHandle Handle)
{
    return ResourceSystem::Get().GetMeshRefCount(Handle);
}

static uint32_t GetShaderRefCount(AxShaderHandle Handle)
{
    return ResourceSystem::Get().GetShaderRefCount(Handle);
}

static uint32_t GetMaterialRefCount(AxMaterialHandle Handle)
{
    return ResourceSystem::Get().GetMaterialRefCount(Handle);
}

static uint32_t GetModelRefCount(AxModelHandle Handle)
{
    return ResourceSystem::Get().GetModelRefCount(Handle);
}

//=============================================================================
// Plugin Entry Points
//=============================================================================

extern "C" {

#if !defined(AX_SHIPPING)
AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry* APIRegistry, bool Load)
{
    if (APIRegistry) {
        APIRegistry->Set(AXON_RESOURCE_API_NAME, &s_ResourceAPI, sizeof(struct AxResourceAPI));
    }
}

AXON_DLL_EXPORT void UnloadPlugin(struct AxAPIRegistry* APIRegistry)
{
    if (APIRegistry) {
        if (ResourceSystem::Get().IsInitialized()) {
            ResourceSystem::Get().Shutdown();
        }
    }
}
#else
void InitAxResource(struct AxAPIRegistry* APIRegistry, bool Load)
{
    if (APIRegistry) {
        APIRegistry->Set(AXON_RESOURCE_API_NAME, &s_ResourceAPI, sizeof(struct AxResourceAPI));
    }
}

void ShutdownAxResource(struct AxAPIRegistry* APIRegistry)
{
    if (APIRegistry) {
        if (ResourceSystem::Get().IsInitialized()) {
            ResourceSystem::Get().Shutdown();
        }
    }
}
#endif

} // extern "C"
