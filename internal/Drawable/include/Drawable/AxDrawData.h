#pragma once

#include "Foundation/Types.h"

struct AxDrawList;

struct AxDrawData
{
    bool Valid;
    size_t TotalIndexCount;
    size_t TotalVertexCount;
    struct AxDrawList *CommandList;
    AxVec2 DisplayPos;
    AxVec2 DisplaySize;
    AxVec2 FramebufferScale;
};

void DrawDataAddDrawList(struct AxDrawData *DrawData, const struct AxDrawList DrawList);
void DrawDataClear(struct AxDrawData *DrawData);