#pragma once

#include "Foundation/AxTypes.h"

struct AxAPIRegistry;

#define AXON_SHADER_MANAGER_API_NAME "AxonShaderManagerAPI"

/**
 * Shader resource handle (type-safe, generation-based)
 * Index 0 is reserved as invalid handle
 */
struct AxShaderHandle
{
    uint32_t Index;      // Index into shader array (0 = invalid)
    uint32_t Generation; // Generation counter to detect stale handles
};

#define AX_INVALID_SHADER_HANDLE (AxShaderHandle{0, 0})

class AxShaderManager
{
public:
    /**
     * Initialize the shader manager with specified capacity
     * @param Registry API registry for accessing other APIs (e.g., OpenGL)
     * @param InitialCapacity Initial number of shader slots
     */
    void Initialize(struct AxAPIRegistry* Registry, uint32_t InitialCapacity);

    /**
     * Shutdown the shader manager and free all resources
     * Warning: All outstanding handles become invalid
     */
    void Shutdown(void);

    /**
     * Create and compile a shader program
     * @param VertexShaderPath Path to vertex shader source
     * @param FragmentShaderPath Path to fragment shader source
     * @return Shader handle (refcount=1), or AX_INVALID_SHADER_HANDLE on failure
     */
    AxShaderHandle CreateShader(const char* VertexShaderPath, const char* FragmentShaderPath);

    /**
     * Get shader data from handle (read-only access)
     * @param Handle Shader handle
     * @return Pointer to shader data, or NULL if handle invalid
     */
    const AxShaderData* GetShaderData(AxShaderHandle Handle);

    /**
     * Get OpenGL shader program ID from handle
     * @param Handle Shader handle
     * @return OpenGL program ID, or 0 if handle invalid
     */
    uint32_t GetShaderProgram(AxShaderHandle Handle);

    /**
     * Get current reference count for a shader (for debugging)
     * @param Handle Shader handle
     * @return Reference count, or 0 if handle invalid
     */
    uint32_t GetRefCount(AxShaderHandle Handle);

    /**
     * Check if a shader handle is valid
     * @param Handle Shader handle to validate
     * @return true if handle is valid, false otherwise
     */
    bool IsValid(AxShaderHandle Handle);

    void Release(AxShaderHandle Handle);

    /**
     * Get total number of active shaders (refcount > 0)
     * @return Number of shaders currently loaded
     */
    uint32_t GetActiveShaderCount(void);

    static AxShaderManager& GetInstance()
    {
        static AxShaderManager *instance = new AxShaderManager();
        return *instance;
    }
};
