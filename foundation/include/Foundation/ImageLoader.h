#pragma once

#include "Foundation/Types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AxImage
{
    int32_t Width;
    int32_t Height;
    int32_t Channels;
    int32_t Size;
    uint8_t *Pixels;
} AxImage;

#define AXON_IMAGE_API_NAME "AxonImageAPI"

struct AxImageAPI
{
    bool (*LoadImage)(const char *Path, AxImage *Image);
};

#if defined(AXON_LINKS_FOUNDATION)
extern struct AxImageAPI *AxImageAPI;
#endif

#ifdef __cplusplus
}
#endif