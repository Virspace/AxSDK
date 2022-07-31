#pragma once

#include "Foundation/AxTypes.h"

// Typical sampling rates for game audio are 48k, sometimes 44.1k
// Mixer thread
// Clips ~10s or longer should probably be streamed
// WASAPI/ASIO

/* Features
* Resampling / Clipping / Limiting
* 3D Panning / Attenuation

*/

typedef enum CompressionType { NONE };

typedef struct AxAudioData
{
    void *Data;
    enum CompressionType Type;
} AxAudioData;

#define AXON_AUDIO_API_NAME "AxoAudioAPI"

struct AxAudioAPI
{
    void (*Init)(void);
    void (*Update)(void);
    void (*Term)(void);

    void (*LoadSound)(const char *SoundName, bool IsLooping, bool ShouldStream);
    void (*UnloadSound)(const char *SoundName);
    void (*Set3DListener)(const AxVec3 *Position, const AxVec3 *Look, const AxVec3 *Up);
    int32_t (*PlaySound)(const char *SoundName);
};