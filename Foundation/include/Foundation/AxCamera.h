#pragma once

#include "AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

enum AxCameraMode {
    AX_CAMERA_MODE_PERSPECTIVE,
    AX_CAMERA_MODE_ORTHOGRAPHIC
};

struct AxCamera;

struct AxCameraAPI
{
    AxMat4x4Inv (*CalcOrthographicProjection)(float Left, float Right, float Bottom, float Top, float Near, float Far);
    AxMat4x4Inv (*CalcPerspectiveProjection)(float AspectRatio, float FocalLength, float NearClip, float FarClip);
};

#define AXON_CAMERA_API_NAME "AxCameraAPI"

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxCameraAPI *CameraAPI;
#endif

#ifdef __cplusplus
}
#endif