#pragma once

#include "Foundation/Types.h"
#include "Foundation/Intrinsics.h"

inline int32_t GetRectWidth(AxRect Rect)
{
    return (Rect.Right - Rect.Left);
}

inline int32_t GetRectHeight(AxRect Rect)
{
    return (Rect.Bottom - Rect.Top);
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
            Result.Right = WindowWidth;

            float Empty = (float)WindowHeight - OptimalWindowHeight;
            int32_t HalfEmpty = RoundFloatToInt32(0.5f * Empty);
            int32_t UseHeight = RoundFloatToInt32(OptimalWindowHeight);

            Result.Bottom = HalfEmpty;
            Result.Top = Result.Bottom * UseHeight;
        }
        else
        {
            Result.Bottom = 0;
            Result.Top = WindowHeight;

            float Empty = (float)WindowWidth - OptimalWindowWidth;
            int32_t HalfEmpty = RoundFloatToInt32(0.5f * Empty);
            int32_t UseWidth = RoundFloatToInt32(OptimalWindowWidth);

            Result.Left = HalfEmpty;
            Result.Right = Result.Left + UseWidth;
        }
    }

    return (Result);
}

inline AxVec2 Vec2Add(AxVec2 A, AxVec2 B)
{
    AxVec2 Result = {
        A.X + B.X,
        A.Y + B.Y
    };

    return (Result);
}

inline AxVec2 Vec2Sub(AxVec2 A, AxVec2 B)
{
    AxVec2 Result = {
        A.X - B.X,
        A.Y - B.Y
    };

    return (Result);
}

inline AxVec2 Vec2Neg(AxVec2 V)
{
    return ((AxVec2){ -V.X, -V.Y });
}

inline AxVec2 Vec2MulS(AxVec2 V, double S)
{
    return ((AxVec2){ V.X * S, V.Y * S });
}

inline AxVec2 Vec2DivS(AxVec2 V, double S)
{
    return ((AxVec2){ V.X / S, V.Y / S });
}

inline double Vec2Len(AxVec2 V)
{
    return (sqrt(V.X * V.X + V.Y * V.Y));
}

inline double Vec2Mag(AxVec2 V)
{
    return (Vec2Len(V));
}

AxVec2 Vec2Norm(AxVec2 V)
{
    return (Vec2MulS(V, (1.0f / Vec2Len(V))));
}

inline AxVec2 Vec2WeightedAvg3(AxVec2 A, AxVec2 B, AxVec2 C, double a, double b, double c)
{
    double Weight = a + b + c;
    return ((AxVec2) {
        (A.X * a + B.X * b + C.X * c) / Weight,
        (A.Y * a + B.Y * b + C.Y * c) / Weight
    });
}

inline AxVec3 Vec3Mul(AxVec3 Vector, double Value)
{
    AxVec3 Result;

    Result.X = Value * Vector.X;
    Result.Y = Value * Vector.Y;
    Result.Z = Value * Vector.Z;

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

static AxVec3 Vec3Add(AxVec3 A, AxVec3 B)
{
    AxVec3 Result;
    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;

    return (Result);
}

inline AxVec3 Vec3WeightedAvg3(AxVec3 A, AxVec3 B, AxVec3 C, double a, double b, double c)
{
    double Weight = a + b + c;
    return ((AxVec3) {
        (A.X * a + B.X * b + C.X * c) / Weight,
        (A.Y * a + B.Y * b + C.Y * c) / Weight,
        (A.Z * a + B.Z * b + C.Z * c) / Weight
    });
}

static AxVec4 Transform(AxMat4x4 A, AxVec4 P)
{
    // NOTE(mdeforge): This is instructive, not optiminal
    AxVec4 R;
    R.X = P.X * A.E[0][0] + P.Y * A.E[0][1] + P.Z * A.E[0][2] + P.W * A.E[0][3];
    R.Y = P.X * A.E[1][0] + P.Y * A.E[1][1] + P.Z * A.E[1][2] + P.W * A.E[1][3];
    R.Z = P.X * A.E[2][0] + P.Y * A.E[2][1] + P.Z * A.E[2][2] + P.W * A.E[2][3];
    R.W = P.X * A.E[3][0] + P.Y * A.E[3][1] + P.Z * A.E[3][2] + P.W * A.E[3][3];

    return (R);
}

inline AxVec4 Mat4x4MulVec4(AxMat4x4 A, AxVec4 P)
{
    AxVec4 R = Transform(A, P);
    return(R);
}

static AxMat4x4 Mat4x4Mul(AxMat4x4 A, AxMat4x4 B)
{
    AxMat4x4 R = {0};
    for (int r = 0; r <= 3; ++r)
    {
        for (int c  = 0; c <= 3; ++c)
        {
            for (int i = 0; i <= 3; ++i) {
                R.E[r][c] += A.E[r][i] * B.E[i][c];
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

inline AxMat4x4 XRotation(double Angle)
{
    double c = Cos(Angle);
    double s = Sin(Angle);

    AxMat4x4 R =
    {
        {{ 1, 0, 0, 0},
         { 0, c, -s, 0},
         { 0, s, c, 0},
         { 0, 0, 0, 1}},
    };

    return (R);
}

inline AxMat4x4 YRotation(double Angle)
{
    double c = Cos(Angle);
    double s = Sin(Angle);

    AxMat4x4 R =
    {
        {{ c, 0, s, 0},
         { 0, 1, 0, 0},
         { -s, 0, c, 0},
         { 0, 0, 0, 1}},
    };

    return (R);
}

inline AxMat4x4 ZRotation(double Angle)
{
    double c = Cos(Angle);
    double s = Sin(Angle);

    AxMat4x4 R =
    {
        {{ c, -s, 0, 0},
         { s, c, 0, 0},
         { 0, 0, 1, 0},
         { 0, 0, 0, 1}},
    };

    return (R);
}

inline double Length(AxVec3 Vector)
{
    double Result = sqrt(Vector.X * Vector.X +
                       Vector.Y * Vector.Y +
                       Vector.Z * Vector.Z);
    return (Result);
}

AxVec3 Normalize(AxVec3 A)
{
    AxVec3 Result = Vec3Mul(A, (1.0 / Length(A)));

    return (Result);
}

inline double DotProduct(AxVec3 A, AxVec3 B)
{
    double Result = A.X * B.X + A.Y * B.Y + A.Z * B.Z;

    return (Result);
}

inline AxVec3 CrossProduct(AxVec3 A, AxVec3 B)
{
    AxVec3 Result;

    Result.X = A.Y * B.Z - A.Z * B.Y;
    Result.Y = A.Z * B.X - A.X * B.Z;
    Result.Z = A.X * B.Y - A.Y * B.X;

    return (Result);
}

AxMat4x4 LookAt(AxVec3 Eye, AxVec3 Center, AxVec3 Up)
{
    const AxVec3 F = Normalize(Vec3Sub(Center, Eye));
    const AxVec3 S = Normalize(CrossProduct(F, Up));
    const AxVec3 U = CrossProduct(S, F);

    AxMat4x4 Result = {0};
    Result.E[0][0] = S.X;
    Result.E[1][0] = S.Y;
    Result.E[2][0] = S.Z;
    Result.E[3][0] = 1;

    Result.E[0][1] = U.X;
    Result.E[1][1] = U.Y;
    Result.E[2][1] = U.Z;
    Result.E[3][1] = 1;

    Result.E[0][2] = -F.X;
    Result.E[1][2] = -F.Y;
    Result.E[2][2] = -F.Z;
    Result.E[3][2] = 1;

    Result.E[3][0] = -DotProduct(S, Eye);
    Result.E[3][1] = -DotProduct(U, Eye);
    Result.E[3][2] = DotProduct(F, Eye);
    Result.E[3][3] = 1;

    return (Result);
}

inline AxMat4x4Inv PerspectiveProjection(float AspectRatio, float FocalLength, float NearClip, float FarClip)
{
    float a = 1.0f;
    float b = AspectRatio;
    float c = FocalLength;

    float n = NearClip;
    float f = FarClip;

    float d = (n + f) / (n - f);
    float e = (2 * f * n) / (n - f);

    AxMat4x4Inv Result =
    {
        // Forward
        {{{a * c,     0,  0, 0},
          {    0, b * c,  0, 0},
          {    0,     0,  d, e},
          {    0,     0, -1, 0}}},
        // Inverse
        {{{1 / (a * c),           0,     0,  0},
          {          0, 1 / (b * c),     0,  0},
          {          0,           0,     0, -1},
          {          0,           0, 1 / e, d / e}}}
    };

    return (Result);
}

inline AxMat4x4Inv OrthographicProjection(float AspectRatio, float NearClip, float FarClip)
{
    float a = 1.0f;
    float b = AspectRatio;

    float n = NearClip;
    float f = FarClip;

    float d = 2.0f / (n - f);
    float e = (n + f) / (n - f);

    AxMat4x4Inv Result =
    {
        // Forward
        {{{a, 0, 0, 0},
          {0, b, 0, 0},
          {0, 0, d, e},
          {0, 0, 0, 1}}},
        // Inverse
        {{{1 / a,     0,     0,      0},
          {    0, 1 / b,     0,      0},
          {    0,     0, 1 / d, -e / d},
          {    0,     0,     0,    1}}}
    };

    return (Result);
}

inline AxVec2 CalcParabolicTouchPoint(AxVec2 A, AxVec2 B, AxVec2 C, float T)
{
    AxVec2 Q = { (1 - T) * A.X + T * B.X, (1 - T) * A.Y + T * B.Y };
    AxVec2 R = { (1 - T) * B.X + T * C.X, (1 - T) * B.Y + T * C.Y };
    AxVec2 P = { (1 - T) * Q.X + T * R.X, (1 - T) * Q.Y + T * R.Y };

    return (P);
}

inline double CalcRatio2D(AxVec2 A, AxVec2 B, AxVec2 Q)
{
    return ((A.Y - Q.Y) / (A.Y - B.Y));
}

inline double Distance2D(AxVec2 A, AxVec2 B)
{
    return (sqrt(pow(B.X - A.X, 2) + pow(B.Y - A.Y, 2)));
}