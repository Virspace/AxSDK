#pragma once

#include "Types.h"

enum AxCameraMode {
    AX_CAMERA_MODE_PERSPECTIVE,
    AX_CAMERA_MODE_ORTHOGRAPHIC
};

struct AxCamera;

struct AxCameraAPI
{
    AxMat4x4f (*CalcOrthographicProjection)(float Left, float Right, float Bottom, float Top, float Near, float Far);
};

#define AX_CAMERA_API_NAME "AxCameraAPI"

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxCameraAPI *AxCameraAPI;
#endif