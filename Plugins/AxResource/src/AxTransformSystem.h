#pragma once

/**
 * AxTransformSystem.h - Transform System for Hybrid Node Tree + Component Architecture
 *
 * Processes all nodes with transforms (traverses scene RootNode hierarchy).
 * Computes world matrices: WorldMatrix = Parent.WorldMatrix * Local.TRS
 * Uses dirty flags: only recomputes when ForwardMatrixDirty is true.
 * Marks children dirty when parent transform changes.
 * Caches computed matrices in AxTransform.CachedForwardMatrix.
 *
 * Registered at EarlyUpdate phase with high priority (runs first).
 *
 * This is C++ code in the AxScene plugin layer.
 */

#include "AxEngine/AxSystem.h"
#include "AxEngine/AxNode.h"

// Forward declaration
class AxScene;

/**
 * TransformSystem - Updates world matrices from node hierarchy.
 *
 * Unlike most systems that iterate a flat component list, TransformSystem
 * traverses the node tree from the RootNode downward so that parent
 * transforms are always computed before children.
 *
 * The system uses the existing AxTransform dirty flag mechanism:
 * - Only recomputes CachedForwardMatrix when ForwardMatrixDirty is true
 * - Marks children dirty when a parent's transform changes
 * - Stores the result in AxTransform.CachedForwardMatrix
 */
class TransformSystem : public System
{
public:
  TransformSystem();

  void Init() override;
  void Shutdown() override;

  /**
   * Update is called by the scene's system loop.
   * TransformSystem ignores the component list parameter and instead
   * traverses the node hierarchy directly from the scene.
   */
  void Update(float DeltaT, Component** Components, uint32_t Count) override;

  /**
   * Set the scene this system operates on.
   * Must be called before the first Update.
   */
  void SetScene(AxScene* Scene);

private:
  /**
   * Recursively traverse the node hierarchy and update world matrices.
   * @param Current The current node being processed.
   * @param ParentWorldMatrix The parent's computed world matrix.
   * @param ParentWasDirty True if the parent's transform was dirty.
   */
  void UpdateNodeTransforms(Node* Current, const AxMat4x4& ParentWorldMatrix,
                            bool ParentWasDirty);

  AxScene* Scene_;
};
