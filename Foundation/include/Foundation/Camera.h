#pragma once

#include "Types.h"

typedef enum AxCameraMode {
    AX_CAMERA_MODE_PERSPECTIVE,
    AX_CAMERA_MODE_ORTHOGRAPHIC
} AxCameraMode;

typedef struct AxCamera
{
    AxCameraMode Mode;
    float NearPlane;
    float FarPlane;
    float VerticalFOV;
} AxCamera;

#define AX_CAMERA_API_NAME "AxCameraAPI"

struct AxCameraAPI
{
    AxMat4x4 *(*ViewFromTransform)(AxMat4x4 *View, const AxTransform *Transform);

    // Calculates an orthographic projection matrix
    AxMat4x4f (*CalcOrthographicProjection)(float Left, float Right, float Bottom, float Top, float Near, float Far);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxCameraAPI *AxCameraAPI;
#endif