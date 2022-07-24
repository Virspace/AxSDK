#include "Foundation/APIRegistry.h"
#include "Foundation/AxMath.h"
#include "Foundation/AxHashTable.h"
#include "Foundation/Platform.h"
#include "AxAudio.h"
#include <stdio.h>
#include <stdlib.h>

// NOTE(mdeforge): Uncomment to get some leak detection
//#define _CRTDBG_MAP_ALLOC
//#include <stdlib.h>
//#include <crtdbg.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

AxHashTable *SoundMap;
AxHashTable *ChannelMap;

static void Init(void)
{
    SoundMap = CreateTable(16);
    ChannelMap = CreateTable(16);
}

static void Update(void)
{

}

static void Term(void)
{

}

// Use a compound literal to construct an unnamed object of API type in-place
struct AxAudioAPI *WindowAPI = &(struct AxAudioAPI) {
    .Init = Init,
    .Update = Update,
    .Term = Term
};

AXON_DLL_EXPORT void LoadPlugin(struct AxAPIRegistry *APIRegistry, bool Load)
{
    if (APIRegistry)
    {
        //PlatformAPI = APIRegistry->Get(AXON_PLATFORM_API_NAME);

        APIRegistry->Set(AXON_AUDIO_API_NAME, WindowAPI, sizeof(struct AxAudioAPI));
    }
}