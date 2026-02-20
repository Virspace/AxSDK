/**
 * AxRenderSystem.cpp - Render System Implementation
 *
 * Iterates all MeshRenderer components, finds the paired MeshFilter
 * on the same node, and collects renderables for the renderer.
 */

#include "AxRenderSystem.h"
#include "AxStandardComponents.h"
#include "AxEngine/AxNode.h"
#include "Foundation/AxMath.h"

RenderSystem::RenderSystem()
  : System("RenderSystem", SystemPhase::Render, 0, 0)
  , RenderableCount_(0)
  , MeshFilterTypeID_(0)
{
}

void RenderSystem::Init()
{
  RenderableCount_ = 0;
}

void RenderSystem::Shutdown()
{
  RenderableCount_ = 0;
}

void RenderSystem::SetMeshFilterTypeID(uint32_t TypeID)
{
  MeshFilterTypeID_ = TypeID;
}

void RenderSystem::Update(float DeltaT, Component** Components, uint32_t Count)
{
  (void)DeltaT;

  // Clear the renderable list each frame
  RenderableCount_ = 0;

  if (!Components || Count == 0) {
    return;
  }

  for (uint32_t i = 0; i < Count; ++i) {
    if (RenderableCount_ >= AX_MAX_RENDERABLES) {
      break;
    }

    Component* Comp = Components[i];
    if (!Comp || !Comp->IsEnabled()) {
      continue;
    }

    // This component is a MeshRenderer - get the owning node
    MeshRenderer* Renderer = static_cast<MeshRenderer*>(Comp);
    Node* Owner = Renderer->GetOwner();
    if (!Owner) {
      continue;
    }

    // Find the MeshFilter on the same node
    Component* FilterComp = Owner->GetComponent(MeshFilterTypeID_);
    if (!FilterComp) {
      // No MeshFilter paired with this MeshRenderer -- skip
      continue;
    }

    MeshFilter* Filter = static_cast<MeshFilter*>(FilterComp);

    // Build a renderable entry
    RenderableEntry& Entry = Renderables_[RenderableCount_];
    Entry.ModelHandle = Filter->ModelHandle;
    Entry.MaterialHandle = Renderer->MaterialHandle;
    Entry.WorldMatrix = Owner->GetTransform().CachedForwardMatrix;

    RenderableCount_++;
  }
}

const RenderableEntry* RenderSystem::GetRenderables(uint32_t* OutCount) const
{
  if (OutCount) {
    *OutCount = RenderableCount_;
  }

  return (Renderables_);
}
