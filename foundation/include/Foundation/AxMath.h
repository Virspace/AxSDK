#pragma once

#include "Foundation/AxTypes.h"
#include "Foundation/AxIntrinsics.h"
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline float Floor(float Num)
{
    return floor(Num);
}

static inline float Log2(float Num)
{
    return log2(Num);
}

static inline float Max(float A, float B)
{
    return (A > B) ? A : B;
}

static inline float Min(float A, float B)
{
    return (A < B) ? A : B;
}

static inline float GetRectWidth(AxRect Rect)
{
    return (Rect.Right - Rect.Left);
}

static inline float GetRectHeight(AxRect Rect)
{
    return (Rect.Bottom - Rect.Top);
}

static inline void SetRectWidth(AxRect *Rect, float Width)
{
    if (Rect) {
        Rect->Right = Rect->Left + Width;
    }
}

static inline void SetRectHeight(AxRect *Rect, float Height)
{
    if (Rect) {
        Rect->Bottom = Rect->Top + Height;
    }
}

static inline void SetRectPosition(AxRect *Rect, float X, float Y)
{
    if (Rect)
    {
        Rect->Left = X;
        Rect->Top = Y;
    }
}

static inline void SetRectSize(AxRect *Rect, float Width, float Height)
{
    if (Rect)
    {
        SetRectWidth(Rect, Width);
        SetRectHeight(Rect, Height);
    }
}

static AxRect AspectRatioFit(uint32_t RenderWidth, uint32_t RenderHeight,
                             uint32_t WindowWidth, uint32_t WindowHeight)
{
    AxRect Result = {0};

    if ((RenderWidth > 0) &&
        (RenderHeight > 0) &&
        (WindowWidth > 0) &&
        (WindowHeight > 0))
    {
        float OptimalWindowHeight = (float)WindowHeight * ((float)RenderWidth / (float)RenderHeight);
        float OptimalWindowWidth = (float)WindowWidth * ((float)RenderHeight / (float)RenderWidth);

        if (OptimalWindowWidth > (float)WindowWidth)
        {
            // NOTE(mdeforge): Width-constrained display -- top and bottom black bars
            Result.Left = 0;
            Result.Right = (float)WindowWidth;

            float Empty = (float)WindowHeight - OptimalWindowHeight;
            int32_t HalfEmpty = RoundFloatToInt32(0.5f * Empty);
            int32_t UseHeight = RoundFloatToInt32(OptimalWindowHeight);

            Result.Bottom = (float)HalfEmpty;
            Result.Top = Result.Bottom * UseHeight;
        }
        else
        {
            Result.Bottom = 0;
            Result.Top = (float)WindowHeight;

            float Empty = (float)WindowWidth - OptimalWindowWidth;
            int32_t HalfEmpty = RoundFloatToInt32(0.5f * Empty);
            int32_t UseWidth = RoundFloatToInt32(OptimalWindowWidth);

            Result.Left = (float)HalfEmpty;
            Result.Right = Result.Left + UseWidth;
        }
    }

    return (Result);
}

static inline AxVec2 Vec2Up()
{
    return ((AxVec2) { 0.0f, 1.0f } );
}

static inline AxVec2 Vec2Add(AxVec2 A, AxVec2 B)
{
    AxVec2 Result = {
        A.X + B.X,
        A.Y + B.Y
    };

    return (Result);
}

static inline AxVec2 Vec2Sub(AxVec2 A, AxVec2 B)
{
    AxVec2 Result = {
        A.X - B.X,
        A.Y - B.Y
    };

    return (Result);
}

static inline AxVec2 Vec2Neg(AxVec2 V)
{
    AxVec2 Result = { -V.X, -V.Y };

    return (Result);
}

static inline AxVec2 Vec2MulS(AxVec2 V, float S)
{
    AxVec2 Result = { V.X * S, V.Y * S };

    return (Result);
}

static inline AxVec2 Vec2DivS(AxVec2 V, float S)
{
    AxVec2 Result = { V.X / S, V.Y / S };

    return (Result);
}

static inline float Vec2Len(AxVec2 V)
{
    return (sqrtf(V.X * V.X + V.Y * V.Y));
}

static inline float Vec2Mag(AxVec2 V)
{
    return (Vec2Len(V));
}

static inline AxVec2 Vec2Norm(AxVec2 V)
{
    return (Vec2MulS(V, (1.0f / Vec2Len(V))));
}

static inline AxVec2 Vec2WeightedAvg3(AxVec2 A, AxVec2 B, AxVec2 C, float a, float b, float c)
{
    float Weight = a + b + c;
    AxVec2 Result = {
        (A.X * a + B.X * b + C.X * c) / Weight,
        (A.Y * a + B.Y * b + C.Y * c) / Weight
    };

    return (Result);
}

static inline AxVec3 Vec3Up()
{
    return ((AxVec3) { 0.0f, 1.0f, 0.0f });
}

static inline AxVec3 Vec3Zero()
{
    return ((AxVec3) { 0.0f, 0.0f, 0.0f });
}


static inline AxVec3 Vec3One()
{
    return ((AxVec3) { 1.0f, 1.0f, 1.0f });
}

static inline AxVec3 Vec3Mul(AxVec3 Vector, float Value)
{
    AxVec3 Result;

    Result.X = Value * Vector.X;
    Result.Y = Value * Vector.Y;
    Result.Z = Value * Vector.Z;

    return (Result);
}

static inline AxVec3 Vec3DivS(AxVec3 V, float S)
{
    return (AxVec3) { V.X / S, V.Y / S, V.Z / S };
}

static AxVec3 Vec3Add(AxVec3 A, AxVec3 B)
{
    AxVec3 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;

    return (Result);
}

static AxVec3 Vec3Sub(AxVec3 A, AxVec3 B)
{
    AxVec3 Result;

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;

    return (Result);
}

static AxVec3 Vec3Neg(AxVec3 V)
{
    return (AxVec3) { -V.X, -V.Y, -V.Z };
}

static inline AxVec3 Vec3WeightedAvg3(AxVec3 A, AxVec3 B, AxVec3 C, float a, float b, float c)
{
    float Weight = a + b + c;
    AxVec3 Result = {
        (A.X * a + B.X * b + C.X * c) / Weight,
        (A.Y * a + B.Y * b + C.Y * c) / Weight,
        (A.Z * a + B.Z * b + C.Z * c) / Weight
    };

    return (Result);
}

static AxVec4 Transform(AxMat4x4 A, AxVec4 P)
{
    // NOTE(mdeforge): Column-major matrix transformation
    AxVec4 R;
    R.X = P.X * A.E[0][0] + P.Y * A.E[1][0] + P.Z * A.E[2][0] + P.W * A.E[3][0];
    R.Y = P.X * A.E[0][1] + P.Y * A.E[1][1] + P.Z * A.E[2][1] + P.W * A.E[3][1];
    R.Z = P.X * A.E[0][2] + P.Y * A.E[1][2] + P.Z * A.E[2][2] + P.W * A.E[3][2];
    R.W = P.X * A.E[0][3] + P.Y * A.E[1][3] + P.Z * A.E[2][3] + P.W * A.E[3][3];

    return (R);
}

static inline AxVec4 Mat4x4MulVec4(AxMat4x4 A, AxVec4 P)
{
    AxVec4 R = Transform(A, P);
    return(R);
}

static AxMat4x4 Mat4x4Mul(AxMat4x4 A, AxMat4x4 B)
{
    AxMat4x4 R = {0};
    for (int c = 0; c <= 3; ++c)
    {
        for (int r = 0; r <= 3; ++r)
        {
            for (int i = 0; i <= 3; ++i) {
                R.E[c][r] += A.E[c][i] * B.E[i][r];
            }
        }
    }

    return (R);
}

static AxMat4x4 Identity(void)
{
    AxMat4x4 R =
    {
        {{1, 0, 0, 0},
         {0, 1, 0, 0},
         {0, 0, 1, 0},
         {0, 0, 0, 1}},
    };

    return (R);
}

static AxMat4x4 Translate(AxMat4x4 A, AxVec3 T)
{
    AxMat4x4 R = A;
    R.E[3][0] += T.X;
    R.E[3][1] += T.Y;
    R.E[3][2] += T.Z;

    return(R);
}

static AxMat4x4 Transpose(const AxMat4x4 Matrix)
{
    AxMat4x4 Result = {0};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j) {
            Result.E[j][i] = Matrix.E[i][j];
        }
    }

    return (Result);
}

static inline AxMat4x4 XRotation(float Angle)
{
    float c = Cos(Angle);
    float s = Sin(Angle);

    AxMat4x4 R =
    {
        {{ 1, 0, 0, 0},
         { 0, c, -s, 0},
         { 0, s, c, 0},
         { 0, 0, 0, 1}},
    };

    return (R);
}

static inline AxMat4x4 YRotation(float Angle)
{
    float c = Cos(Angle);
    float s = Sin(Angle);

    AxMat4x4 R =
    {
        {{ c, 0, s, 0},
         { 0, 1, 0, 0},
         { -s, 0, c, 0},
         { 0, 0, 0, 1}},
    };

    return (R);
}

static inline AxMat4x4 ZRotation(float Angle)
{
    float c = Cos(Angle);
    float s = Sin(Angle);

    AxMat4x4 R =
    {
        {{ c, -s, 0, 0},
         { s, c, 0, 0},
         { 0, 0, 1, 0},
         { 0, 0, 0, 1}},
    };

    return (R);
}

static inline AxMat4x4 MatXRotation(AxMat4x4 A, float Angle)
{
    return (Mat4x4Mul(A, XRotation(Angle)));
}

static inline AxMat4x4 MatYRotation(AxMat4x4 A, float Angle)
{
    return (Mat4x4Mul(A, YRotation(Angle)));
}

static inline AxMat4x4 MatZRotation(AxMat4x4 A, float Angle)
{
    return (Mat4x4Mul(A, ZRotation(Angle)));
}

static inline float Vec3Length(AxVec3 Vector)
{
    float Result = sqrtf(Vector.X * Vector.X +
                       Vector.Y * Vector.Y +
                       Vector.Z * Vector.Z);
    return (Result);
}

static inline AxVec3 Vec3Normalize(AxVec3 A)
{
    AxVec3 Result = Vec3Mul(A, (1.0f / Vec3Length(A)));

    return (Result);
}

static inline float Vec3Dot(AxVec3 A, AxVec3 B)
{
    float Result = A.X * B.X + A.Y * B.Y + A.Z * B.Z;

    return (Result);
}

static inline AxVec3 Vec3Cross(AxVec3 A, AxVec3 B)
{
    AxVec3 Result;

    Result.X = A.Y * B.Z - A.Z * B.Y;
    Result.Y = A.Z * B.X - A.X * B.Z;
    Result.Z = A.X * B.Y - A.Y * B.X;

    return (Result);
}

// Quaternion Math Functions
static inline AxQuat QuatIdentity(void)
{
    return (AxQuat) { .X = 0.0f, .Y = 0.0f, .Z = 0.0f, .W = 1.0f };
}

static inline AxQuat QuatFromAxisAngle(AxVec3 Axis, float AngleRadians)
{
    float HalfAngle = AngleRadians * 0.5f;
    float SinHalf = Sin(HalfAngle);
    float CosHalf = Cos(HalfAngle);

    AxVec3 NormalizedAxis = Vec3Normalize(Axis);

    return (AxQuat) {
        .X = NormalizedAxis.X * SinHalf,
        .Y = NormalizedAxis.Y * SinHalf,
        .Z = NormalizedAxis.Z * SinHalf,
        .W = CosHalf
    };
}

static inline AxQuat QuatFromEuler(AxVec3 Euler)
{
    // Convert Euler angles (in radians) to quaternion
    // Order: Yaw (Y), Pitch (X), Roll (Z)
    float HalfYaw = Euler.Y * 0.5f;
    float HalfPitch = Euler.X * 0.5f;
    float HalfRoll = Euler.Z * 0.5f;

    float CosYaw = Cos(HalfYaw);
    float SinYaw = Sin(HalfYaw);
    float CosPitch = Cos(HalfPitch);
    float SinPitch = Sin(HalfPitch);
    float CosRoll = Cos(HalfRoll);
    float SinRoll = Sin(HalfRoll);

    return (AxQuat) {
        .X = SinPitch * CosYaw * CosRoll - CosPitch * SinYaw * SinRoll,
        .Y = CosPitch * SinYaw * CosRoll + SinPitch * CosYaw * SinRoll,
        .Z = CosPitch * CosYaw * SinRoll - SinPitch * SinYaw * CosRoll,
        .W = CosPitch * CosYaw * CosRoll + SinPitch * SinYaw * SinRoll
    };
}

static inline AxQuat QuatMultiply(AxQuat A, AxQuat B)
{
    return (AxQuat) {
        .X = A.W * B.X + A.X * B.W + A.Y * B.Z - A.Z * B.Y,
        .Y = A.W * B.Y - A.X * B.Z + A.Y * B.W + A.Z * B.X,
        .Z = A.W * B.Z + A.X * B.Y - A.Y * B.X + A.Z * B.W,
        .W = A.W * B.W - A.X * B.X - A.Y * B.Y - A.Z * B.Z
    };
}

static inline AxQuat QuatConjugate(AxQuat Q)
{
    return (AxQuat) { .X = -Q.X, .Y = -Q.Y, .Z = -Q.Z, .W = Q.W };
}

static inline float QuatLength(AxQuat Q)
{
    return sqrtf(Q.X * Q.X + Q.Y * Q.Y + Q.Z * Q.Z + Q.W * Q.W);
}

static inline AxQuat QuatNormalize(AxQuat Q)
{
    float Len = QuatLength(Q);
    if (Len > 0.0f) {
        float InvLen = 1.0f / Len;
        return (AxQuat) { .X = Q.X * InvLen, .Y = Q.Y * InvLen, .Z = Q.Z * InvLen, .W = Q.W * InvLen };
    }
    return QuatIdentity();
}

static inline AxVec3 QuatToEuler(AxQuat Q)
{
    // Normalize quaternion first
    Q = QuatNormalize(Q);

    AxVec3 Euler;

    // Calculate pitch (X-axis rotation)
    float SinRCosP = 2.0f * (Q.W * Q.X + Q.Y * Q.Z);
    float CosRCosP = 1.0f - 2.0f * (Q.X * Q.X + Q.Y * Q.Y);
    Euler.X = atan2(SinRCosP, CosRCosP);

    // Calculate yaw (Y-axis rotation)
    float SinP = 2.0f * (Q.W * Q.Y - Q.Z * Q.X);
    if (fabsf(SinP) >= 1.0f) {
        Euler.Y = copysignf(AX_PI / 2.0f, SinP); // Use 90 degrees if out of range
    } else {
        Euler.Y = asinf(SinP);
    }

    // Calculate roll (Z-axis rotation)
    float SinYCosP = 2.0f * (Q.W * Q.Z + Q.X * Q.Y);
    float CosYCosP = 1.0f - 2.0f * (Q.Y * Q.Y + Q.Z * Q.Z);
    Euler.Z = atan2(SinYCosP, CosYCosP);

    return Euler;
}

static inline AxMat4x4 QuatToMat4x4(AxQuat Q)
{
    // Normalize quaternion first
    Q = QuatNormalize(Q);

    float XX = Q.X * Q.X;
    float YY = Q.Y * Q.Y;
    float ZZ = Q.Z * Q.Z;
    float XY = Q.X * Q.Y;
    float XZ = Q.X * Q.Z;
    float YZ = Q.Y * Q.Z;
    float WX = Q.W * Q.X;
    float WY = Q.W * Q.Y;
    float WZ = Q.W * Q.Z;

    AxMat4x4 Result = {0};

    // Column 0 (Right vector)
    Result.E[0][0] = 1.0f - 2.0f * (YY + ZZ);
    Result.E[0][1] = 2.0f * (XY + WZ);
    Result.E[0][2] = 2.0f * (XZ - WY);
    Result.E[0][3] = 0.0f;

    // Column 1 (Up vector)
    Result.E[1][0] = 2.0f * (XY - WZ);
    Result.E[1][1] = 1.0f - 2.0f * (XX + ZZ);
    Result.E[1][2] = 2.0f * (YZ + WX);
    Result.E[1][3] = 0.0f;

    // Column 2 (Forward vector)
    Result.E[2][0] = 2.0f * (XZ + WY);
    Result.E[2][1] = 2.0f * (YZ - WX);
    Result.E[2][2] = 1.0f - 2.0f * (XX + YY);
    Result.E[2][3] = 0.0f;

    // Column 3 (Translation - will be set separately)
    Result.E[3][0] = 0.0f;
    Result.E[3][1] = 0.0f;
    Result.E[3][2] = 0.0f;
    Result.E[3][3] = 1.0f;

    return (Result);
}

AxQuat Mat4x4ToQuat(AxMat4x4 Matrix);

// Transform System Functions
static inline AxTransform TransformIdentity(void)
{
    return (AxTransform) {
        .Translation = { 0.0f, 0.0f, 0.0f },
        .Rotation = QuatIdentity(),
        .Scale = { 1.0f, 1.0f, 1.0f },
        .Parent = NULL,
        .CachedForwardMatrix = Identity(),
        .CachedInverseMatrix = Identity(),
        .LastComputedFrame = 0,
        .ForwardMatrixDirty = false,
        .InverseMatrixDirty = false,
        .IsIdentity = true
    };
}

static inline void TransformMarkDirty(AxTransform *Transform)
{
    if (!Transform) { return; }

    Transform->ForwardMatrixDirty = true;
    Transform->InverseMatrixDirty = true;
    Transform->IsIdentity = false;
}

static inline void TransformSetTranslation(AxTransform *Transform, AxVec3 Translation)
{
    if (!Transform) { return; }

    Transform->Translation = Translation;
    TransformMarkDirty(Transform);
}

static inline void TransformSetRotation(AxTransform *Transform, AxQuat Rotation)
{    if (!Transform) { return; }

    Transform->Rotation = QuatNormalize(Rotation);
    TransformMarkDirty(Transform);
}

static inline void TransformSetScale(AxTransform *Transform, AxVec3 Scale)
{
    if (!Transform) { return; }

    Transform->Scale = Scale;
    TransformMarkDirty(Transform);
}

// Forward declarations for the lazy computation functions
AxMat4x4 TransformGetForwardMatrix(struct AxTransform *Transform, uint64_t CurrentFrame);
AxMat4x4 TransformGetInverseMatrix(struct AxTransform *Transform, uint64_t CurrentFrame);

void TransformLookAt(struct AxTransform *Transform, AxVec3 targetPosition, AxVec3 up);

static inline AxMat4x4 CalcOrthographicProjection(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    float WInv = 1.0f / (Right - Left);
    float HInv = 1.0f / (Top - Bottom);
    float DInv = 1.0f / (Far - Near);

    AxMat4x4 Result = { 0 };

    // Orthographic projection matrix (column-major)
    Result.E[0][0] = 2.0f * WInv;
    Result.E[3][0] = -(Right + Left) * WInv;

    Result.E[1][1] = 2.0f * HInv;
    Result.E[3][1] = -(Top + Bottom) * HInv;

    Result.E[2][2] = DInv;
    Result.E[3][2] = -Near * DInv;
    Result.E[3][3] = 1.0f;

    return Result;
}

static inline AxMat4x4 CalcPerspectiveProjection(float FOV, float AspectRatio, float NearClip, float FarClip)
{
    float TanHalfFOV_Y = tan(FOV / 2.0f);

    AxMat4x4 Result = {0};

    // Perspective projection matrix (column-major)
    Result.E[0][0] = 1.0f / (AspectRatio * TanHalfFOV_Y);
    Result.E[1][1] = 1.0f / TanHalfFOV_Y;
    Result.E[2][2] = -(FarClip + NearClip) / (FarClip - NearClip);
    Result.E[2][3] = -1.0f;  // Fixed: W component should be in [2][3] for column-major
    Result.E[3][2] = -(2.0f * FarClip * NearClip) / (FarClip - NearClip);
    Result.E[3][3] = 0.0f;

    return Result;
}

// Helper functions for inverse projection matrices (for ray casting, mouse picking, etc.)
static inline AxMat4x4 CalcOrthographicInverseProjection(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    AxMat4x4 Result = { 0 };

    // Inverse orthographic projection matrix (column-major)
    float Width = Right - Left;
    float Height = Top - Bottom;
    float Depth = Far - Near;

    Result.E[0][0] = Width * 0.5f;
    Result.E[3][0] = (Right + Left) * 0.5f;

    Result.E[1][1] = Height * 0.5f;
    Result.E[3][1] = (Top + Bottom) * 0.5f;

    Result.E[2][2] = Depth;
    Result.E[3][2] = Near;
    Result.E[3][3] = 1.0f;

    return Result;
}

static inline AxMat4x4 CalcPerspectiveInverseProjection(float FOV, float AspectRatio, float NearClip, float FarClip)
{
    float TanHalfFOV_Y = tan(FOV / 2.0f);

    AxMat4x4 Result = {0};

    // Inverse perspective projection matrix (column-major)
    Result.E[0][0] = AspectRatio * TanHalfFOV_Y;
    Result.E[1][1] = TanHalfFOV_Y;
    Result.E[2][2] = 0.0f;
    Result.E[3][2] = -1.0f;
    Result.E[2][3] = (NearClip - FarClip) / (2.0f * FarClip * NearClip);
    Result.E[3][3] = (FarClip + NearClip) / (2.0f * FarClip * NearClip);

    return Result;
}

// Column-major 4x4 scale matrix
static inline AxMat4x4 Mat4x4Scale(AxVec3 scale)
{
    AxMat4x4 m = Identity();

    m.E[0][0] = scale.X;  // Scale X
    m.E[1][1] = scale.Y;  // Scale Y
    m.E[2][2] = scale.Z;  // Scale Z
    m.E[3][3] = 1.0f;     // Homogeneous w stays 1

    // Ensure no stray values
    m.E[0][1] = m.E[0][2] = m.E[0][3] = 0.0f;
    m.E[1][0] = m.E[1][2] = m.E[1][3] = 0.0f;
    m.E[2][0] = m.E[2][1] = m.E[2][3] = 0.0f;
    m.E[3][0] = m.E[3][1] = m.E[3][2] = 0.0f;

    return m;
}


static inline AxVec2 CalcParabolicTouchPoint(AxVec2 A, AxVec2 B, AxVec2 C, float T)
{
    AxVec2 Q = { (1 - T) * A.X + T * B.X, (1 - T) * A.Y + T * B.Y };
    AxVec2 R = { (1 - T) * B.X + T * C.X, (1 - T) * B.Y + T * C.Y };
    AxVec2 P = { (1 - T) * Q.X + T * R.X, (1 - T) * Q.Y + T * R.Y };

    return (P);
}

static inline float CalcRatio2D(AxVec2 A, AxVec2 B, AxVec2 Q)
{
    return ((A.Y - Q.Y) / (A.Y - B.Y));
}

static inline float Distance2D(AxVec2 A, AxVec2 B)
{
    return (sqrtf(powf(B.X - A.X, 2.0) + powf(B.Y - A.Y, 2.0)));
}

/**
 * Checks if a value is a power of two.
 * @param Value The value to check.
 * @return True if the value is a power of two, otherwise false.
 */
static inline bool IsPowerOfTwo(size_t Value)
{
    return ((Value & (Value - 1)) == 0);
}

/**
 * Rounds a value up to the nearest power of two multiple.
 * @param Value The value to round up.
 * @param PowerOfTwo The power of two to round up to.
 * @return The nearest power of two multiple, zero if Multiple is not a power of two.
 */
static inline size_t RoundUpToPowerOfTwo(size_t Value, size_t Multiple)
{
    if (!IsPowerOfTwo(Multiple)) {
        return (0);
    }

    return ((Value + Multiple - 1) & ~(Multiple - 1));
}

/**
 * Rounds a value down to the nearest power of two multiple.
 * @param Value The value to round down.
 * @param Multiple The power of two to round down to.
 * @return The nearest power of two multiple, zero if Multiple is not a power of two.
 */
static inline size_t RoundDownToPowerOfTwo(size_t Value, size_t Multiple)
{
    if (!IsPowerOfTwo(Multiple)) {
        return (0);
    }

    return (Value & ~(Multiple - 1));
}

void SeedRandom(uint32_t Seed);

float RandomFloat(const float Min, const float Max);

// Calculate tangent and bitangent vectors for normal mapping
// Given three vertices of a triangle, calculates tangent and bitangent vectors
void CalculateTangentBitangent(
    AxVec3 pos1, AxVec3 pos2, AxVec3 pos3,
    AxVec2 uv1, AxVec2 uv2, AxVec2 uv3,
    AxVec3 *tangent, AxVec3 *bitangent
);

static inline void TransformTranslate(AxTransform *Transform, AxVec3 Translation, bool WorldSpace)
{
    if (!Transform) {
        return;
    }

    if (WorldSpace) {
        Transform->Translation = Vec3Add(Transform->Translation, Translation);
    } else {
        // Transform translation by current rotation
        AxMat4x4 RotMatrix = QuatToMat4x4(Transform->Rotation);
        AxVec4 LocalTranslation = { Translation.X, Translation.Y, Translation.Z, 0.0f };
        AxVec4 WorldTranslation = Mat4x4MulVec4(RotMatrix, LocalTranslation);
        Transform->Translation = Vec3Add(Transform->Translation, 
            (AxVec3){ WorldTranslation.X, WorldTranslation.Y, WorldTranslation.Z });
    }
    TransformMarkDirty(Transform);
}

static inline void TransformRotate(AxTransform *Transform, AxVec3 EulerAngles, bool WorldSpace)
{
    if (!Transform) {
        return;
    }

    AxQuat Rotation = QuatFromEuler(EulerAngles);
    if (WorldSpace) {
        Transform->Rotation = QuatMultiply(Rotation, Transform->Rotation);
    } else {
        Transform->Rotation = QuatMultiply(Transform->Rotation, Rotation);
    }
    TransformMarkDirty(Transform);
}

static inline AxVec3 TransformForward(const AxTransform *Transform)
{
    if (!Transform) {
        return (AxVec3) { 0.0f, 0.0f, -1.0f };
    }

    AxMat4x4 RotMatrix = QuatToMat4x4(Transform->Rotation);
    return (AxVec3){ -RotMatrix.E[2][0], -RotMatrix.E[2][1], -RotMatrix.E[2][2] };
}

static inline AxVec3 TransformRight(const AxTransform *Transform)
{
    if (!Transform) {
        return (AxVec3) { 1.0f, 0.0f, 0.0f };
    }

    AxMat4x4 RotMatrix = QuatToMat4x4(Transform->Rotation);
    return (AxVec3){ RotMatrix.E[0][0], RotMatrix.E[0][1], RotMatrix.E[0][2] };
}

static inline AxVec3 TransformUp(const AxTransform *Transform)
{
    if (!Transform) {
        return (AxVec3) { 0.0f, 1.0f, 0.0f };
    }

    AxMat4x4 RotMatrix = QuatToMat4x4(Transform->Rotation);
    return (AxVec3){ RotMatrix.E[1][0], RotMatrix.E[1][1], RotMatrix.E[1][2] };
}

static inline void TransformRotateFromMouseDelta(AxTransform *Transform, AxVec2 MouseDelta, float Sensitivity)
{
    if (!Transform) {
        return;
    }

    // Calculate yaw (Y rotation) and pitch (X rotation)
    // Invert Y to match standard FPS mouse look (moving mouse up = look up)
    float Yaw = -MouseDelta.X * Sensitivity;
    float Pitch = -MouseDelta.Y * Sensitivity;

    // Apply yaw around world Y axis, pitch around local X axis
    AxQuat YawRotation = QuatFromAxisAngle((AxVec3){ 0.0f, 1.0f, 0.0f }, Yaw);
    AxQuat PitchRotation = QuatFromAxisAngle((AxVec3){ 1.0f, 0.0f, 0.0f }, Pitch);

    // Apply yaw first (world space), then pitch (local space)
    Transform->Rotation = QuatMultiply(YawRotation, Transform->Rotation);
    Transform->Rotation = QuatMultiply(Transform->Rotation, PitchRotation);

    TransformMarkDirty(Transform);
}

static inline float GetAxis(bool Positive, bool Negative)
{
    if (Positive && !Negative) {
        return (1.0f);
    }
    if (Negative && !Positive) {
        return (-1.0f);
    }
    return (0.0f);
}

static inline float Lerp(float A, float B, float T)
{
    return (A + T * (B - A));
}

static inline AxVec3 Vec3Lerp(AxVec3 A, AxVec3 B, float T)
{
    return (AxVec3){
        Lerp(A.X, B.X, T),
        Lerp(A.Y, B.Y, T),
        Lerp(A.Z, B.Z, T)
    };
}

static inline AxQuat QuaternionSlerp(AxQuat A, AxQuat B, float T)
{
    float Dot = A.X * B.X + A.Y * B.Y + A.Z * B.Z + A.W * B.W;

    // If dot product is negative, negate one quaternion to take shorter path
    if (Dot < 0.0f) {
        B.X = -B.X;
        B.Y = -B.Y;
        B.Z = -B.Z;
        B.W = -B.W;
        Dot = -Dot;
    }

    if (Dot > 0.9995f) {
        // Quaternions are very close, use linear interpolation
        AxQuat Result = {
            Lerp(A.X, B.X, T),
            Lerp(A.Y, B.Y, T),
            Lerp(A.Z, B.Z, T),
            Lerp(A.W, B.W, T)
        };
        return (QuatNormalize(Result));
    }

    float Angle = acosf(Dot);
    float SinAngle = sinf(Angle);
    float t1 = sinf((1.0f - T) * Angle) / SinAngle;
    float t2 = sinf(T * Angle) / SinAngle;

    return (AxQuat){
        A.X * t1 + B.X * t2,
        A.Y * t1 + B.Y * t2,
        A.Z * t1 + B.Z * t2,
        A.W * t1 + B.W * t2
    };
}

static inline AxMat4x4 CreateViewMatrix(const AxTransform *CameraTransform)
{
    if (!CameraTransform) {
        return (Identity());
    }

    AxVec3 Forward = TransformForward(CameraTransform);
    AxVec3 Right = TransformRight(CameraTransform);
    AxVec3 Up = TransformUp(CameraTransform);

    AxMat4x4 ViewMatrix = {0};

    // Set rotation part (transpose of camera basis for view matrix)
    ViewMatrix.E[0][0] = Right.X;
    ViewMatrix.E[1][0] = Right.Y;
    ViewMatrix.E[2][0] = Right.Z;

    ViewMatrix.E[0][1] = Up.X;
    ViewMatrix.E[1][1] = Up.Y;
    ViewMatrix.E[2][1] = Up.Z;

    ViewMatrix.E[0][2] = -Forward.X;
    ViewMatrix.E[1][2] = -Forward.Y;
    ViewMatrix.E[2][2] = -Forward.Z;

    // Set translation part (column 3)
    ViewMatrix.E[3][0] = -Vec3Dot(Right, CameraTransform->Translation);
    ViewMatrix.E[3][1] = -Vec3Dot(Up, CameraTransform->Translation);
    ViewMatrix.E[3][2] = Vec3Dot(Forward, CameraTransform->Translation);
    ViewMatrix.E[3][3] = 1.0f;

    return (ViewMatrix);
}

#ifdef __cplusplus
}
#endif