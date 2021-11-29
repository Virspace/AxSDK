#include "Camera.h"

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

    // AxMat4x4f Result = { 0 };
    // Result.E[0][0] = 2.0f / (Right - Left);
    // Result.E[1][1] = 2.0f / (Top - Bottom);
    // Result.E[2][2] = -1.0f; //-2.0f / (Far - Near);
    // Result.E[3][0] = (Right + Left) / (Left - Right); //-(Right + Left) / (Right - Left);
    // Result.E[3][1] = (Top + Bottom) / (Bottom - Top);
    // Result.E[3][2] = 0.0f;
    // Result.E[3][3] = 1.0f;

    return (Result);
}

#if defined(AXON_LINKS_FOUNDATION)

struct AxCameraAPI *AxCameraAPI = &(struct AxCameraAPI) {
    .CalcOrthographicProjection = CalcOrthographicProjection
};

#endif