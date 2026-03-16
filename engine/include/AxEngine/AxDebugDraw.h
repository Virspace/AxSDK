#pragma once

#include "AxEngine/AxMathTypes.h"
#include "AxOpenGL/AxOpenGLTypes.h"

#include <cstdint>
#include <vector>

struct AxOpenGLAPI;

/**
 * DebugVertex - Minimal vertex for debug line rendering.
 *
 * Position (world-space) + Color (RGBA). 28 bytes per vertex.
 * Intentionally different from AxVertex — no normals, texcoords, or tangents.
 */
struct DebugVertex
{
  Vec3 Position;
  Vec4 Color;
};

static_assert(sizeof(DebugVertex) == 28, "DebugVertex must be 28 bytes");

/**
 * DebugDraw - Immediate-mode debug line drawing.
 *
 * Submit primitives (Line, Ray, Box, Sphere) each frame. All geometry
 * is decomposed into line segments and batched into a single GL_LINES
 * draw call. The buffer is cleared after each flush — no cleanup needed.
 *
 * In shipping builds, all methods are no-ops.
 */
class DebugDraw
{
public:
  DebugDraw() = default;
  ~DebugDraw() = default;

  // === Static API (preferred — use from anywhere) ===
  static void DrawLine(const Vec3& From, const Vec3& To, const Vec4& Color);
  static void DrawRay(const Vec3& Origin, const Vec3& Direction, float Length, const Vec4& Color);
  static void DrawBox(const Vec3& Center, const Vec3& HalfExtents, const Vec4& Color);
  static void DrawSphere(const Vec3& Center, float Radius, const Vec4& Color, int Segments = 16);

  // === Instance methods (used internally by engine) ===

  // Primitive submission (world-space coordinates)
  void Line(const Vec3& From, const Vec3& To, const Vec4& Color);
  void Ray(const Vec3& Origin, const Vec3& Direction, float Length, const Vec4& Color);
  void Box(const Vec3& Center, const Vec3& HalfExtents, const Vec4& Color);
  void Sphere(const Vec3& Center, float Radius, const Vec4& Color, int Segments = 16);

  // Buffer management
  void Clear();
  void SetMaxLineVertices(size_t Max) { MaxLineVertices_ = Max; }

  // GPU lifecycle
  void Initialize(AxOpenGLAPI* RenderAPI);
  void Flush(const AxMat4x4& ViewMatrix, const AxMat4x4& ProjectionMatrix);
  void Shutdown();

  // Test accessors
  size_t GetLineVertexCount() const { return (LineVertices_.size()); }
  const std::vector<DebugVertex>& GetLineVertices() const { return (LineVertices_); }

private:
  static DebugDraw* Instance_;

  std::vector<DebugVertex> LineVertices_;
  size_t MaxLineVertices_ = 65536; // 32K line segments

  // GPU resources
  AxDynamicBuffer LineBuffer_ = {};
  uint32_t ShaderProgram_ = 0;
  AxOpenGLAPI* RenderAPI_ = nullptr;
};
