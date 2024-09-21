#pragma once

#include "Foundation/AxTypes.h"

extern struct AxPlatformAPI *AxPlatformAPI;
extern struct AxAPIRegistry *AxGlobalAPIRegistry;
extern struct AxPluginAPI   *AxPluginAPI;

typedef struct AxScene
{
   char *Name;
} Scene;

typedef struct AxProject
{
   char *Name;
   char *Scene;
} Project;