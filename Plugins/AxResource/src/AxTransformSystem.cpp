/**
 * AxTransformSystem.cpp - Transform System Implementation
 *
 * Traverses the node hierarchy from the RootNode downward, computing
 * world matrices as WorldMatrix = Parent.WorldMatrix * Local.TRS.
 * Uses dirty flags to skip clean transforms and propagates dirty
 * state to children when a parent changes.
 */

#include "AxTransformSystem.h"
#include "AxEngine/AxScene.h"
#include "Foundation/AxMath.h"

#include <cstring>

TransformSystem::TransformSystem()
  : System("TransformSystem", SystemPhase::EarlyUpdate, 0, 0)
  , Scene_(nullptr)
{
}

void TransformSystem::Init()
{
  // No initialization needed
}

void TransformSystem::Shutdown()
{
  Scene_ = nullptr;
}

void TransformSystem::SetScene(AxScene* Scene)
{
  Scene_ = Scene;
}

void TransformSystem::Update(float DeltaT, Component** Components, uint32_t Count)
{
  (void)DeltaT;
  (void)Components;
  (void)Count;

  if (!Scene_ || !Scene_->GetRootNode()) {
    return;
  }

  // Start traversal from the root node with an identity parent matrix
  RootNode* Root = Scene_->GetRootNode();
  AxMat4x4 RootWorldMatrix = Identity();
  bool RootDirty = Root->GetTransform().ForwardMatrixDirty;

  // Update root transform first
  AxTransform& RootTransform = Root->GetTransform();
  if (RootTransform.ForwardMatrixDirty) {
    RootTransform.CachedForwardMatrix = Identity();
    RootTransform.ForwardMatrixDirty = false;
  }

  // Traverse children of root
  Node* Child = Root->GetFirstChild();
  while (Child) {
    UpdateNodeTransforms(Child, RootTransform.CachedForwardMatrix, RootDirty);
    Child = Child->GetNextSibling();
  }
}

void TransformSystem::UpdateNodeTransforms(Node* Current,
                                           const AxMat4x4& ParentWorldMatrix,
                                           bool ParentWasDirty)
{
  if (!Current) {
    return;
  }

  AxTransform& Transform = Current->GetTransform();

  // If parent was dirty, this node is also dirty
  if (ParentWasDirty) {
    Transform.ForwardMatrixDirty = true;
  }

  bool ThisNodeDirty = Transform.ForwardMatrixDirty;

  if (ThisNodeDirty) {
    // Compute local TRS matrix using Foundation math
    AxMat4x4 LocalMatrix = {0};

    // Build the TRS matrix: Scale -> Rotate -> Translate
    AxMat4x4 RotMatrix = QuatToMat4x4(Transform.Rotation);

    // Apply scale to rotation columns
    LocalMatrix.E[0][0] = RotMatrix.E[0][0] * Transform.Scale.X;
    LocalMatrix.E[0][1] = RotMatrix.E[0][1] * Transform.Scale.X;
    LocalMatrix.E[0][2] = RotMatrix.E[0][2] * Transform.Scale.X;
    LocalMatrix.E[0][3] = 0.0f;

    LocalMatrix.E[1][0] = RotMatrix.E[1][0] * Transform.Scale.Y;
    LocalMatrix.E[1][1] = RotMatrix.E[1][1] * Transform.Scale.Y;
    LocalMatrix.E[1][2] = RotMatrix.E[1][2] * Transform.Scale.Y;
    LocalMatrix.E[1][3] = 0.0f;

    LocalMatrix.E[2][0] = RotMatrix.E[2][0] * Transform.Scale.Z;
    LocalMatrix.E[2][1] = RotMatrix.E[2][1] * Transform.Scale.Z;
    LocalMatrix.E[2][2] = RotMatrix.E[2][2] * Transform.Scale.Z;
    LocalMatrix.E[2][3] = 0.0f;

    LocalMatrix.E[3][0] = Transform.Translation.X;
    LocalMatrix.E[3][1] = Transform.Translation.Y;
    LocalMatrix.E[3][2] = Transform.Translation.Z;
    LocalMatrix.E[3][3] = 1.0f;

    // WorldMatrix = Parent.WorldMatrix * Local.TRS
    Transform.CachedForwardMatrix = Mat4x4Mul(ParentWorldMatrix, LocalMatrix);
    Transform.ForwardMatrixDirty = false;
  }

  // Recurse to children, passing this node's world matrix
  Node* Child = Current->GetFirstChild();
  while (Child) {
    UpdateNodeTransforms(Child, Transform.CachedForwardMatrix, ThisNodeDirty);
    Child = Child->GetNextSibling();
  }
}
