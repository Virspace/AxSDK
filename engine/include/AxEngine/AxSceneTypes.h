#pragma once

/**
 * AxSceneTypes.h - Engine-Level Scene Type Definitions
 *
 * Defines scene-specific data types that are engine concerns, not
 * fundamental math/utility types. These live at the Engine layer rather
 * than Foundation because they describe scene-level concepts (lights,
 * cameras) that are only meaningful in an engine context.
 *
 * This header is C-compatible so it can be included by C rendering
 * plugins (AxOpenGL) as well as C++ engine code.
 */

#include "Foundation/AxTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Light types supported by the scene system
 */
typedef enum AxLightType {
    AX_LIGHT_TYPE_DIRECTIONAL = 0,
    AX_LIGHT_TYPE_POINT,
    AX_LIGHT_TYPE_SPOT
} AxLightType;

/**
 * Light represents a light source in the scene.
 * Lights can be directional, point, or spot lights.
 */
typedef struct AxLight {
    char Name[64];                      // Light identifier
    AxLightType Type;                   // Type of light
    AxVec3 Position;                    // Position in world space (for point/spot lights)
    AxVec3 Direction;                   // Direction vector (for directional/spot lights)
    AxVec3 Color;                       // Light color (RGB)
    float Intensity;                    // Light intensity multiplier
    float Range;                        // Range for point/spot lights (0 = infinite)
    float InnerConeAngle;               // Inner cone angle for spot lights (radians)
    float OuterConeAngle;               // Outer cone angle for spot lights (radians)
} AxLight;

typedef struct AxCamera
{
    AxTransform Transform;      // Camera transform (position, rotation, scale)
    bool IsOrthographic;        // Orthographic or projection
    float FieldOfView;          // Field of view for perspective projection
    float ZoomLevel;            // Zoom level for orthographic projection
    float AspectRatio;          // Aspect ratio for the camera
    float NearClipPlane;        // Near clipping plane
    float FarClipPlane;         // Far clipping plane
    AxMat4x4 ViewMatrix;        // View matrix
    AxMat4x4 ProjectionMatrix;  // Projection matrix
} AxCamera;

#ifdef __cplusplus
}
#endif
