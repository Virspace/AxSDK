#pragma once

typedef struct ImGuiContext ImGuiContext;
typedef struct EditorPlugin
{
    void (*Init)(ImGuiContext *Context);
    void (*Tick)(void);
    void (*Term)(void);
} EditorPlugin;