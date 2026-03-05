/**
 * AxPrimitives.cpp - Procedural Primitive Mesh Generation (Class Hierarchy)
 *
 * Implements GenerateGeometry() for five primitive shapes (box, sphere, plane,
 * cylinder, capsule). PrimitiveMesh::CreateModel() handles the common pipeline:
 * generate geometry -> create mesh via ResourceAPI -> wrap in model -> return handle.
 */

#include "AxEngine/AxPrimitives.h"
#include "AxOpenGL/AxOpenGLTypes.h"
#include "AxResource/AxResource.h"
#include "AxResource/AxResourceTypes.h"
#include "Foundation/AxAPIRegistry.h"
#include "Foundation/AxMath.h"

#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdio>

//=============================================================================
// Constants
//=============================================================================

static constexpr float PI = 3.14159265358979323846f;

//=============================================================================
// PrimitiveMesh - Static State & Base Implementation
//=============================================================================

AxAPIRegistry* PrimitiveMesh::Registry_ = nullptr;

void PrimitiveMesh::Init(AxAPIRegistry* Registry)
{
    Registry_ = Registry;
}

AxModelHandle PrimitiveMesh::CreateModel()
{
    GeneratedMeshData Data = GenerateGeometry();

    if (!Registry_ || !Data.Vertices || !Data.Indices) {
        free(Data.Vertices);
        free(Data.Indices);
        return (AX_INVALID_HANDLE);
    }

    AxResourceAPI* ResAPI = static_cast<AxResourceAPI*>(Registry_->Get(AXON_RESOURCE_API_NAME));
    if (!ResAPI || !ResAPI->IsInitialized()) {
        free(Data.Vertices);
        free(Data.Indices);
        return (AX_INVALID_HANDLE);
    }

    char Path[128];
    BuildSyntheticPath(Path, sizeof(Path));

    AxMeshHandle MeshHandle = ResAPI->CreateMeshFromData(
        Data.Vertices, Data.Indices, Data.VertexCount, Data.IndexCount, GetPrimitiveName());

    free(Data.Vertices);
    free(Data.Indices);

    if (!AX_HANDLE_IS_VALID(MeshHandle)) {
        return (AX_INVALID_HANDLE);
    }

    Dirty_ = false;

    AxModelHandle ModelHandle = ResAPI->CreateModelFromMesh(MeshHandle, GetPrimitiveName(), Path);
    return (ModelHandle);
}

//=============================================================================
// BoxMesh
//=============================================================================

void BoxMesh::BuildSyntheticPath(char* Buf, size_t Len) const
{
    snprintf(Buf, Len, "primitive://box/%.2f/%.2f/%.2f", Width_, Height_, Depth_);
}

GeneratedMeshData BoxMesh::GenerateGeometry() const
{
    GeneratedMeshData Result = {};
    Result.VertexCount = 24;
    Result.IndexCount = 36;
    Result.Vertices = static_cast<AxVertex*>(malloc(sizeof(AxVertex) * Result.VertexCount));
    Result.Indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * Result.IndexCount));

    if (!Result.Vertices || !Result.Indices) {
        free(Result.Vertices);
        free(Result.Indices);
        Result = {};
        return (Result);
    }

    float HW = Width_ * 0.5f;
    float HH = Height_ * 0.5f;
    float HD = Depth_ * 0.5f;

    struct FaceData {
        AxVec3 Normal;
        AxVec4 Tangent;
        AxVec3 Corners[4];
        AxVec2 UVs[4];
    };

    FaceData Faces[6] = {
        // +X face (right)
        { {1,0,0}, {0,0,-1,1.0f},
          {{HW,-HH,HD}, {HW,-HH,-HD}, {HW,HH,-HD}, {HW,HH,HD}},
          {{0,0}, {1,0}, {1,1}, {0,1}} },
        // -X face (left)
        { {-1,0,0}, {0,0,1,1.0f},
          {{-HW,-HH,-HD}, {-HW,-HH,HD}, {-HW,HH,HD}, {-HW,HH,-HD}},
          {{0,0}, {1,0}, {1,1}, {0,1}} },
        // +Y face (top)
        { {0,1,0}, {1,0,0,1.0f},
          {{-HW,HH,-HD}, {-HW,HH,HD}, {HW,HH,HD}, {HW,HH,-HD}},
          {{0,0}, {0,1}, {1,1}, {1,0}} },
        // -Y face (bottom)
        { {0,-1,0}, {1,0,0,1.0f},
          {{-HW,-HH,HD}, {-HW,-HH,-HD}, {HW,-HH,-HD}, {HW,-HH,HD}},
          {{0,0}, {0,1}, {1,1}, {1,0}} },
        // +Z face (front)
        { {0,0,1}, {1,0,0,1.0f},
          {{-HW,-HH,HD}, {HW,-HH,HD}, {HW,HH,HD}, {-HW,HH,HD}},
          {{0,0}, {1,0}, {1,1}, {0,1}} },
        // -Z face (back)
        { {0,0,-1}, {-1,0,0,1.0f},
          {{HW,-HH,-HD}, {-HW,-HH,-HD}, {-HW,HH,-HD}, {HW,HH,-HD}},
          {{0,0}, {1,0}, {1,1}, {0,1}} }
    };

    for (uint32_t Face = 0; Face < 6; ++Face) {
        uint32_t VI = Face * 4;
        uint32_t II = Face * 6;

        for (uint32_t Corner = 0; Corner < 4; ++Corner) {
            Result.Vertices[VI + Corner].Position = Faces[Face].Corners[Corner];
            Result.Vertices[VI + Corner].Normal = Faces[Face].Normal;
            Result.Vertices[VI + Corner].TexCoord = Faces[Face].UVs[Corner];
            Result.Vertices[VI + Corner].Tangent = Faces[Face].Tangent;
        }

        Result.Indices[II + 0] = VI + 0;
        Result.Indices[II + 1] = VI + 1;
        Result.Indices[II + 2] = VI + 2;
        Result.Indices[II + 3] = VI + 0;
        Result.Indices[II + 4] = VI + 2;
        Result.Indices[II + 5] = VI + 3;
    }

    return (Result);
}

//=============================================================================
// SphereMesh
//=============================================================================

void SphereMesh::BuildSyntheticPath(char* Buf, size_t Len) const
{
    snprintf(Buf, Len, "primitive://sphere/%.2f/%u/%u", Radius_, Rings_, Segments_);
}

GeneratedMeshData SphereMesh::GenerateGeometry() const
{
    GeneratedMeshData Result = {};

    Result.VertexCount = (Rings_ + 1) * (Segments_ + 1);
    Result.IndexCount = Rings_ * Segments_ * 6;
    Result.Vertices = static_cast<AxVertex*>(malloc(sizeof(AxVertex) * Result.VertexCount));
    Result.Indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * Result.IndexCount));

    if (!Result.Vertices || !Result.Indices) {
        free(Result.Vertices);
        free(Result.Indices);
        Result = {};
        return (Result);
    }

    uint32_t VI = 0;
    for (uint32_t Ring = 0; Ring <= Rings_; ++Ring) {
        float V = static_cast<float>(Ring) / static_cast<float>(Rings_);
        float Phi = V * PI;

        for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
            float U = static_cast<float>(Seg) / static_cast<float>(Segments_);
            float Theta = U * 2.0f * PI;

            float SinPhi = sinf(Phi);
            float CosPhi = cosf(Phi);
            float SinTheta = sinf(Theta);
            float CosTheta = cosf(Theta);

            AxVec3 Normal = { SinPhi * CosTheta, CosPhi, SinPhi * SinTheta };

            AxVec3 TangentDir = { -SinTheta, 0.0f, CosTheta };
            float TLen = sqrtf(TangentDir.X * TangentDir.X +
                               TangentDir.Y * TangentDir.Y +
                               TangentDir.Z * TangentDir.Z);
            if (TLen > 0.0001f) {
                TangentDir.X /= TLen;
                TangentDir.Y /= TLen;
                TangentDir.Z /= TLen;
            }

            Result.Vertices[VI].Position = { Normal.X * Radius_, Normal.Y * Radius_, Normal.Z * Radius_ };
            Result.Vertices[VI].Normal = Normal;
            Result.Vertices[VI].TexCoord = { U, V };
            Result.Vertices[VI].Tangent = { TangentDir.X, TangentDir.Y, TangentDir.Z, 1.0f };
            ++VI;
        }
    }

    uint32_t II = 0;
    for (uint32_t Ring = 0; Ring < Rings_; ++Ring) {
        for (uint32_t Seg = 0; Seg < Segments_; ++Seg) {
            uint32_t Current = Ring * (Segments_ + 1) + Seg;
            uint32_t Next = Current + Segments_ + 1;

            Result.Indices[II++] = Current;
            Result.Indices[II++] = Current + 1;
            Result.Indices[II++] = Next;

            Result.Indices[II++] = Current + 1;
            Result.Indices[II++] = Next + 1;
            Result.Indices[II++] = Next;
        }
    }

    return (Result);
}

//=============================================================================
// PlaneMesh
//=============================================================================

void PlaneMesh::BuildSyntheticPath(char* Buf, size_t Len) const
{
    snprintf(Buf, Len, "primitive://plane/%.2f/%.2f/%u/%u",
             Width_, Depth_, SubdivisionsX_, SubdivisionsZ_);
}

GeneratedMeshData PlaneMesh::GenerateGeometry() const
{
    GeneratedMeshData Result = {};

    Result.VertexCount = (SubdivisionsX_ + 1) * (SubdivisionsZ_ + 1);
    Result.IndexCount = SubdivisionsX_ * SubdivisionsZ_ * 6;
    Result.Vertices = static_cast<AxVertex*>(malloc(sizeof(AxVertex) * Result.VertexCount));
    Result.Indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * Result.IndexCount));

    if (!Result.Vertices || !Result.Indices) {
        free(Result.Vertices);
        free(Result.Indices);
        Result = {};
        return (Result);
    }

    float HW = Width_ * 0.5f;
    float HD = Depth_ * 0.5f;

    uint32_t VI = 0;
    for (uint32_t Z = 0; Z <= SubdivisionsZ_; ++Z) {
        float V = static_cast<float>(Z) / static_cast<float>(SubdivisionsZ_);
        float PZ = -HD + V * Depth_;

        for (uint32_t X = 0; X <= SubdivisionsX_; ++X) {
            float U = static_cast<float>(X) / static_cast<float>(SubdivisionsX_);
            float PX = -HW + U * Width_;

            Result.Vertices[VI].Position = { PX, 0.0f, PZ };
            Result.Vertices[VI].Normal = { 0.0f, 1.0f, 0.0f };
            Result.Vertices[VI].TexCoord = { U, V };
            Result.Vertices[VI].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
            ++VI;
        }
    }

    uint32_t II = 0;
    for (uint32_t Z = 0; Z < SubdivisionsZ_; ++Z) {
        for (uint32_t X = 0; X < SubdivisionsX_; ++X) {
            uint32_t TopLeft = Z * (SubdivisionsX_ + 1) + X;
            uint32_t TopRight = TopLeft + 1;
            uint32_t BottomLeft = TopLeft + (SubdivisionsX_ + 1);
            uint32_t BottomRight = BottomLeft + 1;

            Result.Indices[II++] = TopLeft;
            Result.Indices[II++] = BottomLeft;
            Result.Indices[II++] = TopRight;

            Result.Indices[II++] = TopRight;
            Result.Indices[II++] = BottomLeft;
            Result.Indices[II++] = BottomRight;
        }
    }

    return (Result);
}

//=============================================================================
// CylinderMesh
//=============================================================================

void CylinderMesh::BuildSyntheticPath(char* Buf, size_t Len) const
{
    snprintf(Buf, Len, "primitive://cylinder/%.2f/%.2f/%u", Radius_, Height_, Segments_);
}

GeneratedMeshData CylinderMesh::GenerateGeometry() const
{
    GeneratedMeshData Result = {};

    float HH = Height_ * 0.5f;

    uint32_t SideVerts = (Segments_ + 1) * 2;
    uint32_t SideIndices = Segments_ * 6;

    uint32_t CapVerts = 0;
    uint32_t CapIndices = 0;
    if (CapTop_) {
        CapVerts += Segments_ + 2;
        CapIndices += Segments_ * 3;
    }
    if (CapBottom_) {
        CapVerts += Segments_ + 2;
        CapIndices += Segments_ * 3;
    }

    Result.VertexCount = SideVerts + CapVerts;
    Result.IndexCount = SideIndices + CapIndices;
    Result.Vertices = static_cast<AxVertex*>(malloc(sizeof(AxVertex) * Result.VertexCount));
    Result.Indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * Result.IndexCount));

    if (!Result.Vertices || !Result.Indices) {
        free(Result.Vertices);
        free(Result.Indices);
        Result = {};
        return (Result);
    }

    uint32_t VI = 0;
    uint32_t II = 0;

    // Side wall
    for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
        float U = static_cast<float>(Seg) / static_cast<float>(Segments_);
        float Theta = U * 2.0f * PI;
        float CosT = cosf(Theta);
        float SinT = sinf(Theta);

        AxVec3 Normal = { CosT, 0.0f, SinT };
        AxVec4 Tangent = { -SinT, 0.0f, CosT, 1.0f };

        Result.Vertices[VI].Position = { Radius_ * CosT, -HH, Radius_ * SinT };
        Result.Vertices[VI].Normal = Normal;
        Result.Vertices[VI].TexCoord = { U, 0.0f };
        Result.Vertices[VI].Tangent = Tangent;
        ++VI;

        Result.Vertices[VI].Position = { Radius_ * CosT, HH, Radius_ * SinT };
        Result.Vertices[VI].Normal = Normal;
        Result.Vertices[VI].TexCoord = { U, 1.0f };
        Result.Vertices[VI].Tangent = Tangent;
        ++VI;
    }

    for (uint32_t Seg = 0; Seg < Segments_; ++Seg) {
        uint32_t BL = Seg * 2;
        uint32_t TL = Seg * 2 + 1;
        uint32_t BR = (Seg + 1) * 2;
        uint32_t TR = (Seg + 1) * 2 + 1;

        Result.Indices[II++] = BL;
        Result.Indices[II++] = TL;
        Result.Indices[II++] = BR;

        Result.Indices[II++] = TL;
        Result.Indices[II++] = TR;
        Result.Indices[II++] = BR;
    }

    // Top cap
    if (CapTop_) {
        uint32_t CenterIdx = VI;
        Result.Vertices[VI].Position = { 0.0f, HH, 0.0f };
        Result.Vertices[VI].Normal = { 0.0f, 1.0f, 0.0f };
        Result.Vertices[VI].TexCoord = { 0.5f, 0.5f };
        Result.Vertices[VI].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
        ++VI;

        for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
            float Theta = static_cast<float>(Seg) / static_cast<float>(Segments_) * 2.0f * PI;
            float CosT = cosf(Theta);
            float SinT = sinf(Theta);

            Result.Vertices[VI].Position = { Radius_ * CosT, HH, Radius_ * SinT };
            Result.Vertices[VI].Normal = { 0.0f, 1.0f, 0.0f };
            Result.Vertices[VI].TexCoord = { CosT * 0.5f + 0.5f, SinT * 0.5f + 0.5f };
            Result.Vertices[VI].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
            ++VI;
        }

        for (uint32_t Seg = 0; Seg < Segments_; ++Seg) {
            Result.Indices[II++] = CenterIdx;
            Result.Indices[II++] = CenterIdx + 2 + Seg;
            Result.Indices[II++] = CenterIdx + 1 + Seg;
        }
    }

    // Bottom cap
    if (CapBottom_) {
        uint32_t CenterIdx = VI;
        Result.Vertices[VI].Position = { 0.0f, -HH, 0.0f };
        Result.Vertices[VI].Normal = { 0.0f, -1.0f, 0.0f };
        Result.Vertices[VI].TexCoord = { 0.5f, 0.5f };
        Result.Vertices[VI].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
        ++VI;

        for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
            float Theta = static_cast<float>(Seg) / static_cast<float>(Segments_) * 2.0f * PI;
            float CosT = cosf(Theta);
            float SinT = sinf(Theta);

            Result.Vertices[VI].Position = { Radius_ * CosT, -HH, Radius_ * SinT };
            Result.Vertices[VI].Normal = { 0.0f, -1.0f, 0.0f };
            Result.Vertices[VI].TexCoord = { CosT * 0.5f + 0.5f, SinT * 0.5f + 0.5f };
            Result.Vertices[VI].Tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
            ++VI;
        }

        for (uint32_t Seg = 0; Seg < Segments_; ++Seg) {
            Result.Indices[II++] = CenterIdx;
            Result.Indices[II++] = CenterIdx + 1 + Seg;
            Result.Indices[II++] = CenterIdx + 2 + Seg;
        }
    }

    return (Result);
}

//=============================================================================
// CapsuleMesh
//=============================================================================

void CapsuleMesh::BuildSyntheticPath(char* Buf, size_t Len) const
{
    snprintf(Buf, Len, "primitive://capsule/%.2f/%.2f/%u/%u",
             Radius_, Height_, Rings_, Segments_);
}

GeneratedMeshData CapsuleMesh::GenerateGeometry() const
{
    GeneratedMeshData Result = {};

    float TotalHeight = Height_;
    float CylinderHeight = TotalHeight - 2.0f * Radius_;
    if (CylinderHeight < 0.0f) {
        CylinderHeight = 0.0f;
    }
    float HCyl = CylinderHeight * 0.5f;

    // Vertex rows: bottom hemisphere (Rings+1) + cylinder top edge (1) + top hemisphere (Rings)
    // Total unique rows = 2*Rings + 2
    uint32_t TotalRows = 2 * Rings_ + 2;
    Result.VertexCount = (TotalRows + 1) * (Segments_ + 1);
    Result.IndexCount = TotalRows * Segments_ * 6;

    Result.Vertices = static_cast<AxVertex*>(malloc(sizeof(AxVertex) * Result.VertexCount));
    Result.Indices = static_cast<uint32_t*>(malloc(sizeof(uint32_t) * Result.IndexCount));

    if (!Result.Vertices || !Result.Indices) {
        free(Result.Vertices);
        free(Result.Indices);
        Result = {};
        return (Result);
    }

    uint32_t VI = 0;

    // Bottom hemisphere: Rings+1 rows (south pole to equator)
    for (uint32_t Ring = 0; Ring <= Rings_; ++Ring) {
        float T = static_cast<float>(Ring) / static_cast<float>(Rings_);
        float Phi = PI - T * PI * 0.5f; // PI (south pole) to PI/2 (equator)

        float SinPhi = sinf(Phi);
        float CosPhi = cosf(Phi);
        float VCoord = static_cast<float>(Ring) / static_cast<float>(TotalRows);

        for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
            float U = static_cast<float>(Seg) / static_cast<float>(Segments_);
            float Theta = U * 2.0f * PI;
            float CosT = cosf(Theta);
            float SinT = sinf(Theta);

            AxVec3 Normal = { SinPhi * CosT, CosPhi, SinPhi * SinT };

            Result.Vertices[VI].Position = {
                Radius_ * SinPhi * CosT,
                -HCyl + Radius_ * CosPhi,
                Radius_ * SinPhi * SinT
            };
            Result.Vertices[VI].Normal = Normal;
            Result.Vertices[VI].TexCoord = { U, VCoord };
            Result.Vertices[VI].Tangent = { -SinT, 0.0f, CosT, 1.0f };
            ++VI;
        }
    }

    // Cylinder top edge (1 row)
    {
        float VCoord = static_cast<float>(Rings_ + 1) / static_cast<float>(TotalRows);
        for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
            float U = static_cast<float>(Seg) / static_cast<float>(Segments_);
            float Theta = U * 2.0f * PI;
            float CosT = cosf(Theta);
            float SinT = sinf(Theta);

            Result.Vertices[VI].Position = { Radius_ * CosT, HCyl, Radius_ * SinT };
            Result.Vertices[VI].Normal = { CosT, 0.0f, SinT };
            Result.Vertices[VI].TexCoord = { U, VCoord };
            Result.Vertices[VI].Tangent = { -SinT, 0.0f, CosT, 1.0f };
            ++VI;
        }
    }

    // Top hemisphere: Rings rows (equator to north pole)
    for (uint32_t Ring = 1; Ring <= Rings_; ++Ring) {
        float T = static_cast<float>(Ring) / static_cast<float>(Rings_);
        float Phi = PI * 0.5f - T * PI * 0.5f; // PI/2 (equator) to 0 (north pole)

        float SinPhi = sinf(Phi);
        float CosPhi = cosf(Phi);
        float VCoord = static_cast<float>(Rings_ + 1 + Ring) / static_cast<float>(TotalRows);

        for (uint32_t Seg = 0; Seg <= Segments_; ++Seg) {
            float U = static_cast<float>(Seg) / static_cast<float>(Segments_);
            float Theta = U * 2.0f * PI;
            float CosT = cosf(Theta);
            float SinT = sinf(Theta);

            AxVec3 Normal = { SinPhi * CosT, CosPhi, SinPhi * SinT };

            Result.Vertices[VI].Position = {
                Radius_ * SinPhi * CosT,
                HCyl + Radius_ * CosPhi,
                Radius_ * SinPhi * SinT
            };
            Result.Vertices[VI].Normal = Normal;
            Result.Vertices[VI].TexCoord = { U, VCoord };
            Result.Vertices[VI].Tangent = { -SinT, 0.0f, CosT, 1.0f };
            ++VI;
        }
    }

    // Generate indices
    uint32_t II = 0;
    for (uint32_t Row = 0; Row < TotalRows; ++Row) {
        for (uint32_t Seg = 0; Seg < Segments_; ++Seg) {
            uint32_t Current = Row * (Segments_ + 1) + Seg;
            uint32_t Next = Current + Segments_ + 1;

            Result.Indices[II++] = Current;
            Result.Indices[II++] = Next;
            Result.Indices[II++] = Current + 1;

            Result.Indices[II++] = Current + 1;
            Result.Indices[II++] = Next;
            Result.Indices[II++] = Next + 1;
        }
    }

    return (Result);
}
