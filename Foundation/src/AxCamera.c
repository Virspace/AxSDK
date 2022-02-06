#include "AxCamera.h"

struct AxCamera
{
    enum AxCameraMode Mode;
    float NearPlane;
    float FarPlane;
    float VerticalFOV;
};

static AxMat4x4f CalcOrthographicProjection(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    float WInv = 1.0f / (Right - Left);
    float HInv = 1.0f / (Bottom - Top);
    float DInv = 1.0f / (Far - Near);

    AxMat4x4f Result = { 0 };
    Result.E[0][0] = 2.0f * WInv;
    Result.E[0][3] = -(Right + Left) * WInv;
    Result.E[1][1] = 2.0f * HInv;
    Result.E[1][3] = -(Bottom + Top) * HInv;
    Result.E[2][2] = DInv;
    Result.E[2][3] = -Near * DInv;
    Result.E[3][3] = 1.0f;

    return (Result);
}

static AxMat4x4Inv PerspectiveProjection(float AspectRatio, float FocalLength, float NearClip, float FarClip)
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

static AxMat4x4Inv OrthographicProjection(float AspectRatio, float NearClip, float FarClip)
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

// Use a compound literal to construct an unnamed object of API type in-place
struct AxCameraAPI *AxCameraAPI = &(struct AxCameraAPI)
{
    .CalcOrthographicProjection = CalcOrthographicProjection
};