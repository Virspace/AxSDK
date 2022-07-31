#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ImGuiContext;
struct EditorPluginAPI
{
    void (*Init)(struct ImGuiContext *Context);
    void (*Tick)(void);
    void (*Term)(void);
};

#ifdef __cplusplus
}
#endif