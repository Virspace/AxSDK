#include "AxEngine/AxDebugDraw.h"
#include "AxOpenGL/AxOpenGL.h"
#include "AxLog/AxLog.h"

#include <cmath>

#if !defined(AX_SHIPPING)

DebugDraw* DebugDraw::Instance_ = nullptr;

//=============================================================================
// Static API
//=============================================================================

void DebugDraw::DrawLine(const Vec3& From, const Vec3& To, const Vec4& Color)
{
  if (Instance_) { Instance_->Line(From, To, Color); }
}

void DebugDraw::DrawRay(const Vec3& Origin, const Vec3& Direction, float Length, const Vec4& Color)
{
  if (Instance_) { Instance_->Ray(Origin, Direction, Length, Color); }
}

void DebugDraw::DrawBox(const Vec3& Center, const Vec3& HalfExtents, const Vec4& Color)
{
  if (Instance_) { Instance_->Box(Center, HalfExtents, Color); }
}

void DebugDraw::DrawSphere(const Vec3& Center, float Radius, const Vec4& Color, int Segments)
{
  if (Instance_) { Instance_->Sphere(Center, Radius, Color, Segments); }
}

//=============================================================================
// Primitive Submission
//=============================================================================

void DebugDraw::Line(const Vec3& From, const Vec3& To, const Vec4& Color)
{
  if (LineVertices_.size() + 2 > MaxLineVertices_) {
    return;
  }
  LineVertices_.push_back({From, Color});
  LineVertices_.push_back({To, Color});
}

void DebugDraw::Ray(const Vec3& Origin, const Vec3& Direction, float Length, const Vec4& Color)
{
  Vec3 End = Origin + Direction * Length;
  Line(Origin, End, Color);
}

void DebugDraw::Box(const Vec3& Center, const Vec3& HalfExtents, const Vec4& Color)
{
  // A wireframe box has 12 edges = 24 vertices
  if (LineVertices_.size() + 24 > MaxLineVertices_) {
    return;
  }

  float HX = HalfExtents.X;
  float HY = HalfExtents.Y;
  float HZ = HalfExtents.Z;

  // 8 corners of the box
  Vec3 Corners[8] = {
    Center + Vec3(-HX, -HY, -HZ),
    Center + Vec3( HX, -HY, -HZ),
    Center + Vec3( HX,  HY, -HZ),
    Center + Vec3(-HX,  HY, -HZ),
    Center + Vec3(-HX, -HY,  HZ),
    Center + Vec3( HX, -HY,  HZ),
    Center + Vec3( HX,  HY,  HZ),
    Center + Vec3(-HX,  HY,  HZ),
  };

  // 12 edges: bottom face (4), top face (4), vertical pillars (4)
  // Bottom face: 0-1, 1-2, 2-3, 3-0
  LineVertices_.push_back({Corners[0], Color});
  LineVertices_.push_back({Corners[1], Color});
  LineVertices_.push_back({Corners[1], Color});
  LineVertices_.push_back({Corners[2], Color});
  LineVertices_.push_back({Corners[2], Color});
  LineVertices_.push_back({Corners[3], Color});
  LineVertices_.push_back({Corners[3], Color});
  LineVertices_.push_back({Corners[0], Color});

  // Top face: 4-5, 5-6, 6-7, 7-4
  LineVertices_.push_back({Corners[4], Color});
  LineVertices_.push_back({Corners[5], Color});
  LineVertices_.push_back({Corners[5], Color});
  LineVertices_.push_back({Corners[6], Color});
  LineVertices_.push_back({Corners[6], Color});
  LineVertices_.push_back({Corners[7], Color});
  LineVertices_.push_back({Corners[7], Color});
  LineVertices_.push_back({Corners[4], Color});

  // Vertical pillars: 0-4, 1-5, 2-6, 3-7
  LineVertices_.push_back({Corners[0], Color});
  LineVertices_.push_back({Corners[4], Color});
  LineVertices_.push_back({Corners[1], Color});
  LineVertices_.push_back({Corners[5], Color});
  LineVertices_.push_back({Corners[2], Color});
  LineVertices_.push_back({Corners[6], Color});
  LineVertices_.push_back({Corners[3], Color});
  LineVertices_.push_back({Corners[7], Color});
}

void DebugDraw::Sphere(const Vec3& Center, float Radius, const Vec4& Color, int Segments)
{
  int VerticesNeeded = Segments * 2 * 3; // 3 circles, each with Segments line segments
  if (LineVertices_.size() + static_cast<size_t>(VerticesNeeded) > MaxLineVertices_) {
    return;
  }

  float Step = 2.0f * 3.14159265358979f / static_cast<float>(Segments);

  // XY plane (great circle around Z axis)
  for (int i = 0; i < Segments; ++i) {
    float A0 = Step * static_cast<float>(i);
    float A1 = Step * static_cast<float>(i + 1);
    Vec3 P0 = Center + Vec3(cosf(A0) * Radius, sinf(A0) * Radius, 0.0f);
    Vec3 P1 = Center + Vec3(cosf(A1) * Radius, sinf(A1) * Radius, 0.0f);
    LineVertices_.push_back({P0, Color});
    LineVertices_.push_back({P1, Color});
  }

  // XZ plane (great circle around Y axis)
  for (int i = 0; i < Segments; ++i) {
    float A0 = Step * static_cast<float>(i);
    float A1 = Step * static_cast<float>(i + 1);
    Vec3 P0 = Center + Vec3(cosf(A0) * Radius, 0.0f, sinf(A0) * Radius);
    Vec3 P1 = Center + Vec3(cosf(A1) * Radius, 0.0f, sinf(A1) * Radius);
    LineVertices_.push_back({P0, Color});
    LineVertices_.push_back({P1, Color});
  }

  // YZ plane (great circle around X axis)
  for (int i = 0; i < Segments; ++i) {
    float A0 = Step * static_cast<float>(i);
    float A1 = Step * static_cast<float>(i + 1);
    Vec3 P0 = Center + Vec3(0.0f, cosf(A0) * Radius, sinf(A0) * Radius);
    Vec3 P1 = Center + Vec3(0.0f, cosf(A1) * Radius, sinf(A1) * Radius);
    LineVertices_.push_back({P0, Color});
    LineVertices_.push_back({P1, Color});
  }
}

//=============================================================================
// Buffer Management
//=============================================================================

void DebugDraw::Clear()
{
  LineVertices_.clear();
}

//=============================================================================
// GPU Lifecycle
//=============================================================================

static const char* kDebugShaderHeader = "#version 450 core\n";

static const char* kDebugVertexShader =
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec4 aColor;\n"
    "uniform mat4 uView;\n"
    "uniform mat4 uProjection;\n"
    "out vec4 vColor;\n"
    "void main() {\n"
    "    gl_Position = uProjection * uView * vec4(aPosition, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* kDebugFragmentShader =
    "in vec4 vColor;\n"
    "out vec4 FragColor;\n"
    "void main() {\n"
    "    FragColor = vColor;\n"
    "}\n";

void DebugDraw::Initialize(AxOpenGLAPI* RenderAPI)
{
  Instance_ = this;
  RenderAPI_ = RenderAPI;
  if (!RenderAPI_ || !RenderAPI_->CreateDynamicBuffer) {
    AX_LOG(WARNING, "DebugDraw: RenderAPI or CreateDynamicBuffer not available");
    return;
  }

  // Compile debug shader via the existing CreateProgram API
  ShaderProgram_ = RenderAPI_->CreateProgram(
      kDebugShaderHeader, kDebugVertexShader, kDebugFragmentShader);

  // Create dynamic buffer with DebugVertex layout (vec3 Position + vec4 Color)
  AxVertexAttrib Attribs[2] = {
    { 0, 3, 0 },                       // location 0: vec3 at offset 0
    { 1, 4, sizeof(float) * 3 },       // location 1: vec4 at offset 12
  };

  LineBuffer_ = RenderAPI_->CreateDynamicBuffer(
      Attribs, 2, sizeof(DebugVertex),
      static_cast<uint32_t>(MaxLineVertices_));
}

void DebugDraw::Flush(const AxMat4x4& ViewMatrix, const AxMat4x4& ProjectionMatrix)
{
  if (!RenderAPI_ || LineVertices_.empty()) {
    return;
  }

  uint32_t VertexCount = static_cast<uint32_t>(LineVertices_.size());
  uint32_t DataSize = VertexCount * sizeof(DebugVertex);

  RenderAPI_->UpdateBuffer(LineBuffer_, LineVertices_.data(), DataSize);
  RenderAPI_->SetUniformMat4(ShaderProgram_, "uView", &ViewMatrix);
  RenderAPI_->SetUniformMat4(ShaderProgram_, "uProjection", &ProjectionMatrix);
  RenderAPI_->DrawBuffer(LineBuffer_, ShaderProgram_, AX_PRIMITIVE_LINES, VertexCount);
}

void DebugDraw::Shutdown()
{
  Instance_ = nullptr;
  if (RenderAPI_) {
    if (RenderAPI_->DestroyBuffer) {
      RenderAPI_->DestroyBuffer(LineBuffer_);
    }
    if (RenderAPI_->DestroyProgram) {
      RenderAPI_->DestroyProgram(ShaderProgram_);
    }
  }
  LineBuffer_ = {};
  ShaderProgram_ = 0;
  RenderAPI_ = nullptr;
  LineVertices_.clear();
}

#else // AX_SHIPPING — all methods are no-ops

DebugDraw* DebugDraw::Instance_ = nullptr;

void DebugDraw::DrawLine(const Vec3&, const Vec3&, const Vec4&) {}
void DebugDraw::DrawRay(const Vec3&, const Vec3&, float, const Vec4&) {}
void DebugDraw::DrawBox(const Vec3&, const Vec3&, const Vec4&) {}
void DebugDraw::DrawSphere(const Vec3&, float, const Vec4&, int) {}
void DebugDraw::Line(const Vec3&, const Vec3&, const Vec4&) {}
void DebugDraw::Ray(const Vec3&, const Vec3&, float, const Vec4&) {}
void DebugDraw::Box(const Vec3&, const Vec3&, const Vec4&) {}
void DebugDraw::Sphere(const Vec3&, float, const Vec4&, int) {}
void DebugDraw::Clear() {}
void DebugDraw::Initialize(AxOpenGLAPI*) {}
void DebugDraw::Flush(const AxMat4x4&, const AxMat4x4&) {}
void DebugDraw::Shutdown() {}

#endif // AX_SHIPPING
