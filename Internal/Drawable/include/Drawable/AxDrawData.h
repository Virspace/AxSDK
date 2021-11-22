#pragma once

#include "Foundation/Types.h"

typedef struct AxDrawList AxDrawList;

typedef struct AxDrawData
{
    bool Valid;
    size_t TotalIndexCount;
    size_t TotalVertexCount;
    AxDrawList **CommandList;
    AxVec2 DisplaySize;
    AxVec2 FramebufferScale;
} AxDrawData;

void DrawDataAddDrawList(AxDrawData *DrawData, AxDrawList *DrawList);
void DrawDataClear(AxDrawData *DrawData);