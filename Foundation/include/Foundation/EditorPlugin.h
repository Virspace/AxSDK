#pragma once

struct ImGuiContext;
struct EditorPluginAPI
{
    void (*Init)(struct ImGuiContext *Context);
    void (*Tick)(void);
    void (*Term)(void);
}