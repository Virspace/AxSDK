#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "Foundation/AxTypes.h"

struct ImGuiContext;
struct EditorPluginAPI
{
    void (*Init)(struct ImGuiContext *Context);
    bool (*Tick)(void);
    void (*Term)(void);
};

#ifdef __cplusplus
}
#endif