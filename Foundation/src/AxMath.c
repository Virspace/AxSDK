#include "AxMath.h"
#include <stdlib.h>

// TODO(mdeforge): Use better rand function

void SeedRandom(uint32_t Seed)
{
    srand(Seed);
}

float RandomFloat(const float Min, const float Max)
{
    return ((float)rand() / (RAND_MAX) * (Max - Min) + Min);
}

void CalculateTangentBitangent(
    AxVec3 pos1, AxVec3 pos2, AxVec3 pos3,
    AxVec2 uv1, AxVec2 uv2, AxVec2 uv3,
    AxVec3 *tangent, AxVec3 *bitangent)
{
    // Calculate edge vectors
    AxVec3 edge1 = Vec3Sub(pos2, pos1);
    AxVec3 edge2 = Vec3Sub(pos3, pos1);

    // Calculate UV deltas
    AxVec2 deltaUV1 = Vec2Sub(uv2, uv1);
    AxVec2 deltaUV2 = Vec2Sub(uv3, uv1);

    // Calculate tangent and bitangent
    float f = 1.0f / (deltaUV1.X * deltaUV2.Y - deltaUV2.X * deltaUV1.Y);

    if (tangent) {
        tangent->X = f * (deltaUV2.Y * edge1.X - deltaUV1.Y * edge2.X);
        tangent->Y = f * (deltaUV2.Y * edge1.Y - deltaUV1.Y * edge2.Y);
        tangent->Z = f * (deltaUV2.Y * edge1.Z - deltaUV1.Y * edge2.Z);
        *tangent = Vec3Normalize(*tangent);
    }

    if (bitangent) {
        bitangent->X = f * (-deltaUV2.X * edge1.X + deltaUV1.X * edge2.X);
        bitangent->Y = f * (-deltaUV2.X * edge1.Y + deltaUV1.X * edge2.Y);
        bitangent->Z = f * (-deltaUV2.X * edge1.Z + deltaUV1.X * edge2.Z);
        *bitangent = Vec3Normalize(*bitangent);
    }
}

AxQuat Mat4x4ToQuat(AxMat4x4 Matrix)
{
    AxQuat rotation;
    float trace = Matrix.E[0][0] + Matrix.E[1][1] + Matrix.E[2][2];

    if (trace > 0.0f) {
        float s = sqrtf(trace + 1.0f) * 2.0f; // s = 4 * qw
        rotation.W = 0.25f * s;
        rotation.X = (Matrix.E[1][2] - Matrix.E[2][1]) / s;
        rotation.Y = (Matrix.E[2][0] - Matrix.E[0][2]) / s;
        rotation.Z = (Matrix.E[0][1] - Matrix.E[1][0]) / s;
    } else if ((Matrix.E[0][0] > Matrix.E[1][1]) && (Matrix.E[0][0] > Matrix.E[2][2])) {
        float s = sqrtf(1.0f + Matrix.E[0][0] - Matrix.E[1][1] - Matrix.E[2][2]) * 2.0f; // s = 4 * qx
        rotation.W = (Matrix.E[1][2] - Matrix.E[2][1]) / s;
        rotation.X = 0.25f * s;
        rotation.Y = (Matrix.E[1][0] + Matrix.E[0][1]) / s;
        rotation.Z = (Matrix.E[2][0] + Matrix.E[0][2]) / s;
    } else if (Matrix.E[1][1] > Matrix.E[2][2]) {
        float s = sqrtf(1.0f + Matrix.E[1][1] - Matrix.E[0][0] - Matrix.E[2][2]) * 2.0f; // s = 4 * qy
        rotation.W = (Matrix.E[2][0] - Matrix.E[0][2]) / s;
        rotation.X = (Matrix.E[1][0] + Matrix.E[0][1]) / s;
        rotation.Y = 0.25f * s;
        rotation.Z = (Matrix.E[2][1] + Matrix.E[1][2]) / s;
    } else {
        float s = sqrtf(1.0f + Matrix.E[2][2] - Matrix.E[0][0] - Matrix.E[1][1]) * 2.0f; // s = 4 * qz
        rotation.W = (Matrix.E[0][1] - Matrix.E[1][0]) / s;
        rotation.X = (Matrix.E[2][0] + Matrix.E[0][2]) / s;
        rotation.Y = (Matrix.E[2][1] + Matrix.E[1][2]) / s;
        rotation.Z = 0.25f * s;
    }

    return (rotation);
}

void TransformLookAt(struct AxTransform *Transform, AxVec3 targetPosition, AxVec3 up)
{
    if (!Transform) {
        return;
    }

    // Calculate forward vector (direction from transform to target)
    AxVec3 forward = Vec3Normalize(Vec3Sub(targetPosition, Transform->Translation));

    // Calculate right vector (perpendicular to forward and up)
    AxVec3 right = Vec3Normalize(Vec3Cross(forward, up));

    // Recalculate up vector to ensure orthogonality
    AxVec3 newUp = Vec3Cross(right, forward);

    // Create rotation matrix from the orthonormal basis (column-major)
    // Note: In a right-handed coordinate system where forward is -Z
    AxMat4x4 rotationMatrix = {0};

    // First column (right vector)
    rotationMatrix.E[0][0] = right.X;
    rotationMatrix.E[0][1] = right.Y;
    rotationMatrix.E[0][2] = right.Z;
    rotationMatrix.E[0][3] = 0.0f;

    // Second column (up vector)
    rotationMatrix.E[1][0] = newUp.X;
    rotationMatrix.E[1][1] = newUp.Y;
    rotationMatrix.E[1][2] = newUp.Z;
    rotationMatrix.E[1][3] = 0.0f;

    // Third column (negative forward for right-handed system)
    rotationMatrix.E[2][0] = -forward.X;
    rotationMatrix.E[2][1] = -forward.Y;
    rotationMatrix.E[2][2] = -forward.Z;
    rotationMatrix.E[2][3] = 0.0f;

    // Fourth column (no translation in rotation matrix)
    rotationMatrix.E[3][0] = 0.0f;
    rotationMatrix.E[3][1] = 0.0f;
    rotationMatrix.E[3][2] = 0.0f;
    rotationMatrix.E[3][3] = 1.0f;

    // Convert rotation matrix to quaternion
    AxQuat rotation = Mat4x4ToQuat(rotationMatrix);

    // Set the transform's rotation
    TransformSetRotation(Transform, rotation);
}

// Transform System Implementation

static AxMat4x4 ComputeTRSMatrix(AxVec3 Translation, AxQuat Rotation, AxVec3 Scale)
{
    // Create scale matrix
    AxMat4x4 ScaleMatrix = Identity();
    ScaleMatrix.E[0][0] = Scale.X;
    ScaleMatrix.E[1][1] = Scale.Y;
    ScaleMatrix.E[2][2] = Scale.Z;

    // Create rotation matrix from quaternion
    AxMat4x4 RotationMatrix = QuatToMat4x4(Rotation);

    // Create translation matrix (column-major)
    AxMat4x4 TranslationMatrix = Identity();
    TranslationMatrix.E[3][0] = Translation.X;
    TranslationMatrix.E[3][1] = Translation.Y;
    TranslationMatrix.E[3][2] = Translation.Z;

    // Combine: Translation * Rotation * Scale
    AxMat4x4 RS = Mat4x4Mul(RotationMatrix, ScaleMatrix);
    return Mat4x4Mul(TranslationMatrix, RS);
}

static AxMat4x4 ComputeInverseTRSMatrix(AxVec3 Translation, AxQuat Rotation, AxVec3 Scale)
{
    // For inverse TRS: Scale^-1 * Rotation^-1 * Translation^-1

    // Inverse scale (1/scale for each component)
    AxMat4x4 InvScaleMatrix = Identity();
    InvScaleMatrix.E[0][0] = (Scale.X != 0.0f) ? (1.0f / Scale.X) : 1.0f;
    InvScaleMatrix.E[1][1] = (Scale.Y != 0.0f) ? (1.0f / Scale.Y) : 1.0f;
    InvScaleMatrix.E[2][2] = (Scale.Z != 0.0f) ? (1.0f / Scale.Z) : 1.0f;

    // Inverse rotation (quaternion conjugate for unit quaternions)
    AxQuat InvRotation = QuatConjugate(Rotation);
    AxMat4x4 InvRotationMatrix = QuatToMat4x4(InvRotation);

    // Inverse translation (column-major)
    AxMat4x4 InvTranslationMatrix = Identity();
    InvTranslationMatrix.E[3][0] = -Translation.X;
    InvTranslationMatrix.E[3][1] = -Translation.Y;
    InvTranslationMatrix.E[3][2] = -Translation.Z;

    // Combine: Scale^-1 * Rotation^-1 * Translation^-1
    AxMat4x4 SR = Mat4x4Mul(InvScaleMatrix, InvRotationMatrix);
    return Mat4x4Mul(SR, InvTranslationMatrix);
}

AxMat4x4 TransformGetForwardMatrix(AxTransform *Transform, uint64_t CurrentFrame)
{
    if (!Transform) return Identity();

    // Check if we can use cached result
    if (!Transform->ForwardMatrixDirty && Transform->LastComputedFrame == CurrentFrame) {
        return Transform->CachedForwardMatrix;
    }

    // Fast path for identity transforms
    if (Transform->IsIdentity && 
        Transform->Translation.X == 0.0f && Transform->Translation.Y == 0.0f && Transform->Translation.Z == 0.0f &&
        Transform->Rotation.X == 0.0f && Transform->Rotation.Y == 0.0f && Transform->Rotation.Z == 0.0f && Transform->Rotation.W == 1.0f &&
        Transform->Scale.X == 1.0f && Transform->Scale.Y == 1.0f && Transform->Scale.Z == 1.0f) {

        Transform->CachedForwardMatrix = Identity();
        Transform->ForwardMatrixDirty = false;
        Transform->LastComputedFrame = CurrentFrame;
        return Transform->CachedForwardMatrix;
    }

    // Compute local transform matrix
    AxMat4x4 LocalMatrix = ComputeTRSMatrix(Transform->Translation, Transform->Rotation, Transform->Scale);

    // Apply parent transform if it exists
    if (Transform->Parent) {
        AxMat4x4 ParentMatrix = TransformGetForwardMatrix(Transform->Parent, CurrentFrame);
        Transform->CachedForwardMatrix = Mat4x4Mul(ParentMatrix, LocalMatrix);
    } else {
        Transform->CachedForwardMatrix = LocalMatrix;
    }

    // Update cache state
    Transform->ForwardMatrixDirty = false;
    Transform->LastComputedFrame = CurrentFrame;

    return Transform->CachedForwardMatrix;
}

AxMat4x4 TransformGetInverseMatrix(AxTransform *Transform, uint64_t CurrentFrame)
{
    if (!Transform) return Identity();

    // Check if we can use cached result
    if (!Transform->InverseMatrixDirty && Transform->LastComputedFrame == CurrentFrame) {
        return Transform->CachedInverseMatrix;
    }

    // Fast path for identity transforms
    if (Transform->IsIdentity && 
        Transform->Translation.X == 0.0f && Transform->Translation.Y == 0.0f && Transform->Translation.Z == 0.0f &&
        Transform->Rotation.X == 0.0f && Transform->Rotation.Y == 0.0f && Transform->Rotation.Z == 0.0f && Transform->Rotation.W == 1.0f &&
        Transform->Scale.X == 1.0f && Transform->Scale.Y == 1.0f && Transform->Scale.Z == 1.0f) {

        Transform->CachedInverseMatrix = Identity();
        Transform->InverseMatrixDirty = false;
        Transform->LastComputedFrame = CurrentFrame;
        return Transform->CachedInverseMatrix;
    }

    // Compute local inverse transform matrix
    AxMat4x4 LocalInverseMatrix = ComputeInverseTRSMatrix(Transform->Translation, Transform->Rotation, Transform->Scale);

    // Apply parent inverse transform if it exists (note: order is reversed for inverse)
    if (Transform->Parent) {
        AxMat4x4 ParentInverseMatrix = TransformGetInverseMatrix(Transform->Parent, CurrentFrame);
        Transform->CachedInverseMatrix = Mat4x4Mul(LocalInverseMatrix, ParentInverseMatrix);
    } else {
        Transform->CachedInverseMatrix = LocalInverseMatrix;
    }

    // Update cache state
    Transform->InverseMatrixDirty = false;
    Transform->LastComputedFrame = CurrentFrame;

    return Transform->CachedInverseMatrix;
}