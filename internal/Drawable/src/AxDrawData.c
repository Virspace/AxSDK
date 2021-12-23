#include "AxDrawData.h"
#include "AxDrawList.h"
#include "Foundation/AxArray.h"
#include <string.h>

void DrawDataAddDrawList(AxDrawData *DrawData, const AxDrawList DrawList)
{
    DrawData->TotalVertexCount += ArraySize(DrawList.VertexBuffer);
    DrawData->TotalIndexCount += ArraySize(DrawList.IndexBuffer);

    ArrayPush(DrawData->CommandList, DrawList);
}

void DrawDataClear(AxDrawData *DrawData)
{
    for (int i = 0; i < ArraySize(DrawData->CommandList); ++i) {
        DrawListClear(&DrawData->CommandList[i]);
    }

    ArrayFree(DrawData->CommandList);
    memset(&DrawData, 0, sizeof(DrawData));
}