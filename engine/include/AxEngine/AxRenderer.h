#pragma once

#include "Foundation/AxTypes.h"

struct AxAPIRegistry;
struct AxOpenGLAPI;
struct AxResourceAPI;
struct AxViewport;
struct AxShaderData;
struct AxModelData;

class SceneTree;
class Node;
class CameraNode;

/**
 * AxRender - Handles all rendering operations
 *
 * Manages viewport, camera, shaders, and scene rendering.
 * Created and owned by AxEngine.
 */
class AXENGINE_API AxRenderer
{
public:
    AxRenderer() = default;
    ~AxRenderer();

    /**
     * Initialize the renderer.
     * @param Registry API registry for accessing render and resource APIs
     * @param WindowHandle Native window handle for context creation
     * @param Width Initial viewport width
     * @param Height Initial viewport height
     * @return true on success
     */
    bool Initialize(AxAPIRegistry* Registry, uint64_t WindowHandle, int32_t Width, int32_t Height);

    /**
     * Shutdown and release all rendering resources.
     */
    void Shutdown();

    /**
     * Begin a new frame (clear buffers, set viewport).
     */
    void BeginFrame();

    /**
     * End frame (swap buffers).
     */
    void EndFrame();

    /**
     * Render a scene using the main camera.
     * @param Scene SceneTree to render (can be nullptr)
     */
    void RenderScene(SceneTree* Scene);

    /**
     * Get the main camera node.
     */
    CameraNode* GetMainCamera() { return (MainCameraNode_); }

    /**
     * Set the main camera from a scene CameraNode.
     * Initializes OpenGL projection state and sets aspect ratio.
     */
    void SetMainCamera(CameraNode* Camera);

private:
    void RenderNode(Node* NodePtr, const AxMat4x4* ParentTransform);
    void RenderModel(const AxModelData* Model, const AxMat4x4* BaseTransform);

    AxOpenGLAPI* RenderAPI_{nullptr};
    AxResourceAPI* ResourceAPI_{nullptr};

    AxViewport* Viewport_{nullptr};
    CameraNode* MainCameraNode_{nullptr};
    AxShaderData* ShaderData_{nullptr};
};
