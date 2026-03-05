#pragma once

/**
 * AxPrimitives.h - Procedural Primitive Mesh Generation (Class Hierarchy)
 *
 * Provides a Godot-inspired class hierarchy for creating primitive geometric
 * shapes (box, sphere, plane, cylinder, capsule) as AxModelHandle resources.
 * Each subclass has sensible defaults, fluent property setters with dirty
 * tracking, and produces meshes identical to GLTF-loaded models.
 *
 * This is engine-layer C++ code (DEC-014). No function-pointer structs or
 * registry indirection — callers use these classes directly.
 *
 * Usage from C++ scripts (Game.dll):
 *   AxModelHandle Box = BoxMesh().CreateModel();                           // unit box
 *   AxModelHandle Cyl = CylinderMesh().SetRadius(0.3f).SetHeight(0.8f).CreateModel();
 *
 * Usage from .ats scene files:
 *   mesh: primitive box { width: 2.0 height: 1.0 depth: 2.0 }
 */

#include "AxResource/AxResourceTypes.h"

#include <cstdint>
#include <cstddef>

// Forward declarations
struct AxAPIRegistry;
struct AxVertex;

//=============================================================================
// Generated Mesh Data (CPU-side arrays before GPU upload)
//=============================================================================

struct GeneratedMeshData
{
    AxVertex* Vertices;
    uint32_t* Indices;
    uint32_t VertexCount;
    uint32_t IndexCount;
};

//=============================================================================
// PrimitiveMesh - Abstract Base Class
//=============================================================================

class PrimitiveMesh
{
public:
    /** Store the registry pointer so CreateModel() can access ResourceAPI. Called once at startup. */
    static void Init(AxAPIRegistry* Registry);

    virtual ~PrimitiveMesh() = default;

    /** Generate geometry, upload via ResourceAPI, return model handle. */
    AxModelHandle CreateModel();

    /** Dirty tracking for editor integration. */
    bool IsDirty() const { return (Dirty_); }
    void MarkDirty() { Dirty_ = true; }

    /** Generate CPU-side vertex/index data. Subclasses implement this. */
    virtual GeneratedMeshData GenerateGeometry() const = 0;

    /** Human-readable name for this primitive type (e.g. "Box", "Sphere"). */
    virtual const char* GetPrimitiveName() const = 0;

    /** Build a unique synthetic path for caching (e.g. "primitive://box/1.00/1.00/1.00"). */
    virtual void BuildSyntheticPath(char* Buf, size_t Len) const = 0;

protected:
    bool Dirty_ = true;

    static AxAPIRegistry* Registry_;
};

//=============================================================================
// BoxMesh
//=============================================================================

class BoxMesh : public PrimitiveMesh
{
public:
    BoxMesh& SetWidth(float V)  { Width_ = V; MarkDirty(); return (*this); }
    BoxMesh& SetHeight(float V) { Height_ = V; MarkDirty(); return (*this); }
    BoxMesh& SetDepth(float V)  { Depth_ = V; MarkDirty(); return (*this); }

    float GetWidth() const  { return (Width_); }
    float GetHeight() const { return (Height_); }
    float GetDepth() const  { return (Depth_); }

    GeneratedMeshData GenerateGeometry() const override;
    const char* GetPrimitiveName() const override { return ("Box"); }
    void BuildSyntheticPath(char* Buf, size_t Len) const override;

private:
    float Width_ = 1.0f;
    float Height_ = 1.0f;
    float Depth_ = 1.0f;
};

//=============================================================================
// SphereMesh
//=============================================================================

class SphereMesh : public PrimitiveMesh
{
public:
    SphereMesh& SetRadius(float V)       { Radius_ = V; MarkDirty(); return (*this); }
    SphereMesh& SetRings(uint32_t V)     { Rings_ = V; MarkDirty(); return (*this); }
    SphereMesh& SetSegments(uint32_t V)  { Segments_ = V; MarkDirty(); return (*this); }

    float GetRadius() const       { return (Radius_); }
    uint32_t GetRings() const     { return (Rings_); }
    uint32_t GetSegments() const  { return (Segments_); }

    GeneratedMeshData GenerateGeometry() const override;
    const char* GetPrimitiveName() const override { return ("Sphere"); }
    void BuildSyntheticPath(char* Buf, size_t Len) const override;

private:
    float Radius_ = 0.5f;
    uint32_t Rings_ = 16;
    uint32_t Segments_ = 32;
};

//=============================================================================
// PlaneMesh
//=============================================================================

class PlaneMesh : public PrimitiveMesh
{
public:
    PlaneMesh& SetWidth(float V)              { Width_ = V; MarkDirty(); return (*this); }
    PlaneMesh& SetDepth(float V)              { Depth_ = V; MarkDirty(); return (*this); }
    PlaneMesh& SetSubdivisionsX(uint32_t V)   { SubdivisionsX_ = V; MarkDirty(); return (*this); }
    PlaneMesh& SetSubdivisionsZ(uint32_t V)   { SubdivisionsZ_ = V; MarkDirty(); return (*this); }

    float GetWidth() const              { return (Width_); }
    float GetDepth() const              { return (Depth_); }
    uint32_t GetSubdivisionsX() const   { return (SubdivisionsX_); }
    uint32_t GetSubdivisionsZ() const   { return (SubdivisionsZ_); }

    GeneratedMeshData GenerateGeometry() const override;
    const char* GetPrimitiveName() const override { return ("Plane"); }
    void BuildSyntheticPath(char* Buf, size_t Len) const override;

private:
    float Width_ = 1.0f;
    float Depth_ = 1.0f;
    uint32_t SubdivisionsX_ = 1;
    uint32_t SubdivisionsZ_ = 1;
};

//=============================================================================
// CylinderMesh
//=============================================================================

class CylinderMesh : public PrimitiveMesh
{
public:
    CylinderMesh& SetRadius(float V)       { Radius_ = V; MarkDirty(); return (*this); }
    CylinderMesh& SetHeight(float V)       { Height_ = V; MarkDirty(); return (*this); }
    CylinderMesh& SetSegments(uint32_t V)  { Segments_ = V; MarkDirty(); return (*this); }
    CylinderMesh& SetCapTop(bool V)        { CapTop_ = V; MarkDirty(); return (*this); }
    CylinderMesh& SetCapBottom(bool V)     { CapBottom_ = V; MarkDirty(); return (*this); }

    float GetRadius() const       { return (Radius_); }
    float GetHeight() const       { return (Height_); }
    uint32_t GetSegments() const  { return (Segments_); }
    bool GetCapTop() const        { return (CapTop_); }
    bool GetCapBottom() const     { return (CapBottom_); }

    GeneratedMeshData GenerateGeometry() const override;
    const char* GetPrimitiveName() const override { return ("Cylinder"); }
    void BuildSyntheticPath(char* Buf, size_t Len) const override;

private:
    float Radius_ = 0.5f;
    float Height_ = 1.0f;
    uint32_t Segments_ = 32;
    bool CapTop_ = true;
    bool CapBottom_ = true;
};

//=============================================================================
// CapsuleMesh
//=============================================================================

class CapsuleMesh : public PrimitiveMesh
{
public:
    CapsuleMesh& SetRadius(float V)       { Radius_ = V; MarkDirty(); return (*this); }
    CapsuleMesh& SetHeight(float V)       { Height_ = V; MarkDirty(); return (*this); }
    CapsuleMesh& SetRings(uint32_t V)     { Rings_ = V; MarkDirty(); return (*this); }
    CapsuleMesh& SetSegments(uint32_t V)  { Segments_ = V; MarkDirty(); return (*this); }

    float GetRadius() const       { return (Radius_); }
    float GetHeight() const       { return (Height_); }
    uint32_t GetRings() const     { return (Rings_); }
    uint32_t GetSegments() const  { return (Segments_); }

    GeneratedMeshData GenerateGeometry() const override;
    const char* GetPrimitiveName() const override { return ("Capsule"); }
    void BuildSyntheticPath(char* Buf, size_t Len) const override;

private:
    float Radius_ = 0.5f;
    float Height_ = 1.0f;
    uint32_t Rings_ = 8;
    uint32_t Segments_ = 32;
};
