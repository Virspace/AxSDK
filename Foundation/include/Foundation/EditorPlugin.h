#pragma once

typedef struct ImGuiContext ImGuiContext;
typedef struct EditorPluginAPI
{
    void (*Init)(ImGuiContext *Context);
    void (*Tick)(void);
    void (*Term)(void);
} EditorPluginAPI;