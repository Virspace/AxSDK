#include "AxCamera.h"
#include <math.h>

struct AxCamera
{
    enum AxCameraMode Mode;
    float NearPlane;
    float FarPlane;
    float VerticalFOV;
};

static AxMat4x4Inv AxCalcOrthographicProjection(float Left, float Right, float Bottom, float Top, float Near, float Far)
{
    float WInv = 1.0f / (Right - Left);
    float HInv = 1.0f / (Top - Bottom);
    float DInv = 1.0f / (Far - Near);

    AxMat4x4Inv Result = { 0 };

    // Forward projection matrix
    Result.Forward.E[0][0] = 2.0f * WInv;
    Result.Forward.E[0][3] = -(Right + Left) * WInv;

    Result.Forward.E[1][1] = 2.0f * HInv;
    Result.Forward.E[1][3] = -(Top + Bottom) * HInv;

    Result.Forward.E[2][2] = DInv;
    Result.Forward.E[2][3] = -Near * DInv;
    Result.Forward.E[3][3] = 1.0f;

    // Inverse projection matrix
    Result.Inverse.E[0][0] = 1.0f / (2.0f * WInv);  // equivalent to (Right - Left)/2
    Result.Inverse.E[0][3] = (Right + Left) / 2.0f;

    Result.Inverse.E[1][1] = 1.0f / (2.0f * HInv);  // equivalent to (Top - Bottom)/2
    Result.Inverse.E[1][3] = (Top + Bottom) / 2.0f;

    Result.Inverse.E[2][2] = 1.0f / DInv;           // equals Far - Near
    Result.Inverse.E[2][3] = Near;
    Result.Inverse.E[3][3] = 1.0f;

    return Result;
}


static AxMat4x4Inv AxCalcPerspectiveProjection(float FOV, float AspectRatio, float NearClip, float FarClip)
{
    float tanHalfFovy = tan(FOV / 2.0f);

    AxMat4x4Inv Result = {0};

    // Set up perspective projection matrix for OpenGL (column-major)
    Result.Forward.E[0][0] = 1.0f / (AspectRatio * tanHalfFovy);
    Result.Forward.E[1][1] = 1.0f / tanHalfFovy;
    Result.Forward.E[2][2] = -(FarClip + NearClip) / (FarClip - NearClip);
    Result.Forward.E[2][3] = -1.0f;
    Result.Forward.E[3][2] = -(2.0f * FarClip * NearClip) / (FarClip - NearClip);
    Result.Forward.E[3][3] = 0.0f;

    // TODO: Calculate inverse matrix if needed

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
struct AxCameraAPI *CameraAPI = &(struct AxCameraAPI)
{
    .CalcOrthographicProjection = AxCalcOrthographicProjection,
    .CalcPerspectiveProjection = AxCalcPerspectiveProjection
};