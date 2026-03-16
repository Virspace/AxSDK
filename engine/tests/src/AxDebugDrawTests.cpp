/**
 * AxDebugDrawTests.cpp - Tests for DebugDraw CPU-side primitive submission
 *
 * Verifies vertex counts, positions, colors, buffer management,
 * and geometry correctness for Line, Ray, Box, and Sphere primitives.
 */

#include "gtest/gtest.h"
#include "AxEngine/AxDebugDraw.h"
#include "AxEngine/AxMathTypes.h"

#include <cmath>
#include <set>

static constexpr float kEpsilon = 1e-5f;

//=============================================================================
// Line Tests
//=============================================================================

TEST(DebugDrawLine, AddsExactlyTwoVertices)
{
  DebugDraw DD;
  DD.Line({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
  EXPECT_EQ(DD.GetLineVertexCount(), 2u);
}

TEST(DebugDrawLine, VerticesHaveCorrectPositions)
{
  DebugDraw DD;
  Vec3 From(1.0f, 2.0f, 3.0f);
  Vec3 To(4.0f, 5.0f, 6.0f);
  Vec4 Color(1.0f, 0.0f, 0.0f, 1.0f);

  DD.Line(From, To, Color);

  const auto& Verts = DD.GetLineVertices();
  EXPECT_FLOAT_EQ(Verts[0].Position.X, 1.0f);
  EXPECT_FLOAT_EQ(Verts[0].Position.Y, 2.0f);
  EXPECT_FLOAT_EQ(Verts[0].Position.Z, 3.0f);
  EXPECT_FLOAT_EQ(Verts[1].Position.X, 4.0f);
  EXPECT_FLOAT_EQ(Verts[1].Position.Y, 5.0f);
  EXPECT_FLOAT_EQ(Verts[1].Position.Z, 6.0f);
}

TEST(DebugDrawLine, VerticesHaveCorrectColor)
{
  DebugDraw DD;
  Vec4 Color(0.5f, 0.6f, 0.7f, 0.8f);
  DD.Line({0, 0, 0}, {1, 0, 0}, Color);

  const auto& Verts = DD.GetLineVertices();
  EXPECT_FLOAT_EQ(Verts[0].Color.X, 0.5f);
  EXPECT_FLOAT_EQ(Verts[0].Color.Y, 0.6f);
  EXPECT_FLOAT_EQ(Verts[0].Color.Z, 0.7f);
  EXPECT_FLOAT_EQ(Verts[0].Color.W, 0.8f);
  EXPECT_FLOAT_EQ(Verts[1].Color.X, 0.5f);
  EXPECT_FLOAT_EQ(Verts[1].Color.Y, 0.6f);
  EXPECT_FLOAT_EQ(Verts[1].Color.Z, 0.7f);
  EXPECT_FLOAT_EQ(Verts[1].Color.W, 0.8f);
}

TEST(DebugDrawLine, ColorAlphaIsPreserved)
{
  DebugDraw DD;
  Vec4 SemiTransparent(1.0f, 1.0f, 1.0f, 0.25f);
  DD.Line({0, 0, 0}, {1, 0, 0}, SemiTransparent);

  const auto& Verts = DD.GetLineVertices();
  EXPECT_FLOAT_EQ(Verts[0].Color.W, 0.25f);
  EXPECT_FLOAT_EQ(Verts[1].Color.W, 0.25f);
}

//=============================================================================
// Ray Tests
//=============================================================================

TEST(DebugDrawRay, AddsExactlyTwoVertices)
{
  DebugDraw DD;
  DD.Ray({0, 0, 0}, {1, 0, 0}, 5.0f, {1, 0, 0, 1});
  EXPECT_EQ(DD.GetLineVertexCount(), 2u);
}

TEST(DebugDrawRay, EndpointIsOriginPlusDirectionTimesLength)
{
  DebugDraw DD;
  Vec3 Origin(1.0f, 2.0f, 3.0f);
  Vec3 Dir(0.0f, 1.0f, 0.0f);
  float Length = 10.0f;
  DD.Ray(Origin, Dir, Length, {1, 0, 0, 1});

  const auto& Verts = DD.GetLineVertices();
  EXPECT_FLOAT_EQ(Verts[0].Position.X, 1.0f);
  EXPECT_FLOAT_EQ(Verts[0].Position.Y, 2.0f);
  EXPECT_FLOAT_EQ(Verts[0].Position.Z, 3.0f);
  EXPECT_FLOAT_EQ(Verts[1].Position.X, 1.0f);
  EXPECT_FLOAT_EQ(Verts[1].Position.Y, 12.0f);
  EXPECT_FLOAT_EQ(Verts[1].Position.Z, 3.0f);
}

//=============================================================================
// Box Tests
//=============================================================================

TEST(DebugDrawBox, AddsExactly24Vertices)
{
  DebugDraw DD;
  DD.Box({0, 0, 0}, {1, 1, 1}, {1, 1, 0, 1});
  // 12 edges * 2 vertices per edge = 24
  EXPECT_EQ(DD.GetLineVertexCount(), 24u);
}

TEST(DebugDrawBox, AllEightCornersPresent)
{
  DebugDraw DD;
  Vec3 Center(0.0f, 0.0f, 0.0f);
  Vec3 Half(1.0f, 2.0f, 3.0f);
  DD.Box(Center, Half, {1, 1, 1, 1});

  const auto& Verts = DD.GetLineVertices();

  // Collect all unique corners (snap to avoid float noise)
  std::set<std::tuple<int, int, int>> Corners;
  for (size_t i = 0; i < Verts.size(); ++i) {
    int X = static_cast<int>(std::round(Verts[i].Position.X));
    int Y = static_cast<int>(std::round(Verts[i].Position.Y));
    int Z = static_cast<int>(std::round(Verts[i].Position.Z));
    Corners.insert({X, Y, Z});
  }

  // A box has 8 unique corners
  EXPECT_EQ(Corners.size(), 8u);

  // Verify all expected corners exist
  for (int X : {-1, 1}) {
    for (int Y : {-2, 2}) {
      for (int Z : {-3, 3}) {
        EXPECT_TRUE(Corners.count({X, Y, Z}) > 0)
            << "Missing corner (" << X << ", " << Y << ", " << Z << ")";
      }
    }
  }
}

TEST(DebugDrawBox, EachEdgeConnectsTwoCornersOnOneAxis)
{
  DebugDraw DD;
  DD.Box({0, 0, 0}, {1, 1, 1}, {1, 1, 1, 1});

  const auto& Verts = DD.GetLineVertices();
  ASSERT_EQ(Verts.size(), 24u);

  // Each edge (pair of consecutive vertices) should differ in exactly one axis
  for (size_t i = 0; i < Verts.size(); i += 2) {
    const auto& A = Verts[i].Position;
    const auto& B = Verts[i + 1].Position;
    int DiffCount = 0;
    if (std::fabs(A.X - B.X) > kEpsilon) DiffCount++;
    if (std::fabs(A.Y - B.Y) > kEpsilon) DiffCount++;
    if (std::fabs(A.Z - B.Z) > kEpsilon) DiffCount++;
    EXPECT_EQ(DiffCount, 1)
        << "Edge " << (i / 2) << " differs in " << DiffCount << " axes";
  }
}

TEST(DebugDrawBox, NonOriginCenterProducesCorrectCorners)
{
  DebugDraw DD;
  Vec3 Center(5.0f, 5.0f, 5.0f);
  Vec3 Half(1.0f, 1.0f, 1.0f);
  DD.Box(Center, Half, {1, 1, 1, 1});

  const auto& Verts = DD.GetLineVertices();

  // All vertices should be within [4,6] on each axis
  for (size_t i = 0; i < Verts.size(); ++i) {
    EXPECT_GE(Verts[i].Position.X, 4.0f - kEpsilon);
    EXPECT_LE(Verts[i].Position.X, 6.0f + kEpsilon);
    EXPECT_GE(Verts[i].Position.Y, 4.0f - kEpsilon);
    EXPECT_LE(Verts[i].Position.Y, 6.0f + kEpsilon);
    EXPECT_GE(Verts[i].Position.Z, 4.0f - kEpsilon);
    EXPECT_LE(Verts[i].Position.Z, 6.0f + kEpsilon);
  }
}

//=============================================================================
// Sphere Tests
//=============================================================================

TEST(DebugDrawSphere, Default16SegmentsAdds96Vertices)
{
  DebugDraw DD;
  DD.Sphere({0, 0, 0}, 1.0f, {0, 1, 0, 1});
  // 3 great circles * 16 segments * 2 vertices = 96
  EXPECT_EQ(DD.GetLineVertexCount(), 96u);
}

TEST(DebugDrawSphere, CustomSegmentCountScalesVertexCount)
{
  DebugDraw DD;
  DD.Sphere({0, 0, 0}, 1.0f, {0, 1, 0, 1}, 8);
  // 3 great circles * 8 segments * 2 vertices = 48
  EXPECT_EQ(DD.GetLineVertexCount(), 48u);
}

TEST(DebugDrawSphere, VerticesLieOnSphereSurface)
{
  DebugDraw DD;
  Vec3 Center(2.0f, 3.0f, 4.0f);
  float Radius = 5.0f;
  DD.Sphere(Center, Radius, {1, 1, 1, 1}, 16);

  const auto& Verts = DD.GetLineVertices();
  for (size_t i = 0; i < Verts.size(); ++i) {
    float DX = Verts[i].Position.X - Center.X;
    float DY = Verts[i].Position.Y - Center.Y;
    float DZ = Verts[i].Position.Z - Center.Z;
    float Dist = std::sqrt(DX * DX + DY * DY + DZ * DZ);
    EXPECT_NEAR(Dist, Radius, kEpsilon)
        << "Vertex " << i << " distance from center: " << Dist;
  }
}

TEST(DebugDrawSphere, ThreeOrthogonalGreatCirclesPresent)
{
  DebugDraw DD;
  Vec3 Center(0.0f, 0.0f, 0.0f);
  float Radius = 1.0f;
  int Segments = 16;
  DD.Sphere(Center, Radius, {1, 1, 1, 1}, Segments);

  const auto& Verts = DD.GetLineVertices();
  int VerticesPerCircle = Segments * 2;

  // Circle 0 (XY plane): all Z should be ~0
  bool HasXYCircle = true;
  for (int i = 0; i < VerticesPerCircle; ++i) {
    if (std::fabs(Verts[i].Position.Z) > kEpsilon) {
      HasXYCircle = false;
      break;
    }
  }

  // Circle 1 (XZ plane): all Y should be ~0
  bool HasXZCircle = true;
  for (int i = VerticesPerCircle; i < VerticesPerCircle * 2; ++i) {
    if (std::fabs(Verts[i].Position.Y) > kEpsilon) {
      HasXZCircle = false;
      break;
    }
  }

  // Circle 2 (YZ plane): all X should be ~0
  bool HasYZCircle = true;
  for (int i = VerticesPerCircle * 2; i < VerticesPerCircle * 3; ++i) {
    if (std::fabs(Verts[i].Position.X) > kEpsilon) {
      HasYZCircle = false;
      break;
    }
  }

  EXPECT_TRUE(HasXYCircle) << "XY great circle not found";
  EXPECT_TRUE(HasXZCircle) << "XZ great circle not found";
  EXPECT_TRUE(HasYZCircle) << "YZ great circle not found";
}

//=============================================================================
// Accumulation and Buffer Management Tests
//=============================================================================

TEST(DebugDrawBuffer, MultiplePrimitivesAccumulate)
{
  DebugDraw DD;
  DD.Line({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});   // 2 vertices
  DD.Line({0, 0, 0}, {0, 1, 0}, {0, 1, 0, 1});   // 2 vertices
  DD.Box({0, 0, 0}, {1, 1, 1}, {1, 1, 0, 1});    // 24 vertices
  EXPECT_EQ(DD.GetLineVertexCount(), 28u);
}

TEST(DebugDrawBuffer, ClearResetsToZero)
{
  DebugDraw DD;
  DD.Line({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
  DD.Box({0, 0, 0}, {1, 1, 1}, {1, 1, 0, 1});
  EXPECT_GT(DD.GetLineVertexCount(), 0u);

  DD.Clear();
  EXPECT_EQ(DD.GetLineVertexCount(), 0u);
}

TEST(DebugDrawBuffer, AfterClearNewPrimitivesStartFresh)
{
  DebugDraw DD;
  DD.Line({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
  DD.Clear();
  DD.Line({5, 5, 5}, {6, 6, 6}, {0, 0, 1, 1});

  EXPECT_EQ(DD.GetLineVertexCount(), 2u);
  const auto& Verts = DD.GetLineVertices();
  EXPECT_FLOAT_EQ(Verts[0].Position.X, 5.0f);
}

TEST(DebugDrawBuffer, OverflowIsCappedGracefully)
{
  DebugDraw DD;
  DD.SetMaxLineVertices(10);

  // Each line adds 2 vertices, so 5 lines = 10 vertices (at capacity)
  for (int i = 0; i < 5; ++i) {
    DD.Line({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
  }
  EXPECT_EQ(DD.GetLineVertexCount(), 10u);

  // This line should be silently dropped (would exceed capacity)
  DD.Line({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
  EXPECT_EQ(DD.GetLineVertexCount(), 10u);
}

TEST(DebugDrawBuffer, BoxDroppedWhenInsufficientCapacity)
{
  DebugDraw DD;
  DD.SetMaxLineVertices(20);

  // Box needs 24 vertices, but capacity is only 20 — should be dropped
  DD.Box({0, 0, 0}, {1, 1, 1}, {1, 1, 0, 1});
  EXPECT_EQ(DD.GetLineVertexCount(), 0u);
}
