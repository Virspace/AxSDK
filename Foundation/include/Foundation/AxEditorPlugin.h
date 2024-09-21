#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/AxTypes.h"

struct ImGuiContext;
struct EditorPluginAPI
{
    bool (*Init)(struct ImGuiContext *Context);
    bool (*Tick)(void);
    bool (*Term)(void);
};

#ifdef __cplusplus
}
#endif