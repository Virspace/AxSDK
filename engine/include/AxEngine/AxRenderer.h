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
     * Render a scene using the main camera or editor camera.
     * When UseEditorCamera_ is true, uses the stored editor camera
     * view/projection matrices instead of the scene's CameraNode.
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

    /**
     * Resize the viewport and update camera aspect ratio.
     * Called by AxEngine when the editor viewport panel resizes.
     * @param Width New viewport width in pixels
     * @param Height New viewport height in pixels
     */
    void Resize(int32_t Width, int32_t Height);

    /**
     * Set the editor camera view/projection matrices.
     * Computes view matrix from position/target and projection matrix
     * from FOV/near/far/aspect ratio.
     * @param Position Camera position in world space
     * @param Target Look-at target in world space
     * @param FOV Field of view in degrees
     * @param Near Near clip plane distance
     * @param Far Far clip plane distance
     */
    void SetEditorCameraView(AxVec3 Position, AxVec3 Target,
                             float FOV, float Near, float Far);

    /**
     * Enable or disable the editor camera for rendering.
     * When enabled, RenderScene uses the editor camera matrices
     * instead of the scene's MainCameraNode.
     */
    void SetUseEditorCamera(bool Use) { UseEditorCamera_ = Use; }

    /** Query whether the editor camera is currently active. */
    bool IsUsingEditorCamera() const { return (UseEditorCamera_); }

private:
    void RenderNode(Node* NodePtr, const AxMat4x4* ParentTransform);
    void RenderModel(const AxModelData* Model, const AxMat4x4* BaseTransform);

    AxOpenGLAPI* RenderAPI_{nullptr};
    AxResourceAPI* ResourceAPI_{nullptr};

    AxViewport* Viewport_{nullptr};
    CameraNode* MainCameraNode_{nullptr};
    AxShaderData* ShaderData_{nullptr};

    // Editor camera state
    bool UseEditorCamera_{false};
    AxMat4x4 EditorViewMatrix_;
    AxMat4x4 EditorProjectionMatrix_;
    AxVec3 EditorCameraPosition_;
};
