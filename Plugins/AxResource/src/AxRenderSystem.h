#pragma once

/**
 * AxRenderSystem.h - Render System for Hybrid Node Tree + Component Architecture
 *
 * Queries the scene for all MeshRenderer components, finds the paired
 * MeshFilter on each node, and builds a renderable list of
 * (MeshHandle, MaterialHandle, WorldMatrix) tuples for the renderer.
 *
 * Registered at Render phase.
 *
 * This is C++ code in the AxScene plugin layer.
 */

#include "AxEngine/AxSystem.h"
#include "AxEngine/AxComponent.h"
#include "AxResource/AxResourceTypes.h"
#include "Foundation/AxTypes.h"

#include <cstdint>

/**
 * RenderableEntry - A single item to be rendered.
 * Contains the model handle, material handle, and world transform matrix.
 */
struct RenderableEntry
{
  AxModelHandle ModelHandle;
  uint32_t MaterialHandle;
  AxMat4x4 WorldMatrix;
};

/**
 * Maximum number of renderables collected per frame.
 */
#define AX_MAX_RENDERABLES 4096

/**
 * RenderSystem - Collects MeshFilter+MeshRenderer pairs into a renderable list.
 *
 * For each MeshRenderer component in the scene, the system looks up the
 * MeshFilter on the same node. If both exist, a RenderableEntry is
 * added to the output list with the node's cached world matrix.
 *
 * The renderable list is rebuilt each frame during the Render phase.
 */
class RenderSystem : public System
{
public:
  RenderSystem();

  void Init() override;
  void Shutdown() override;

  /**
   * Processes all MeshRenderer components. For each one, looks up the
   * MeshFilter on the same owning node and builds the renderable list.
   */
  void Update(float DeltaT, Component** Components, uint32_t Count) override;

  /**
   * Get the renderable list collected during the last Update.
   * @param OutCount Receives the number of renderables.
   * @return Pointer to the array of RenderableEntry structs.
   */
  const RenderableEntry* GetRenderables(uint32_t* OutCount) const;

  /**
   * Set the TypeID used to look up MeshFilter components on nodes.
   * Must be set after standard component registration.
   */
  void SetMeshFilterTypeID(uint32_t TypeID);

private:
  RenderableEntry Renderables_[AX_MAX_RENDERABLES];
  uint32_t RenderableCount_;
  uint32_t MeshFilterTypeID_;
};
