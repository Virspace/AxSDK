#include "AxDrawData.h"
#include "AxDrawList.h"
#include "Foundation/AxArray.h"
#include <string.h>

void DrawDataAddDrawList(AxDrawData *DrawData, AxDrawList *DrawList)
{
    DrawData->TotalVertexCount += ArraySize(DrawList->VertexBuffer);
    DrawData->TotalIndexCount += ArraySize(DrawList->IndexBuffer);
    ArrayPush(DrawData->CommandList, DrawList);
}

void DrawDataClear(AxDrawData *DrawData)
{
    memset(&DrawData, 0, sizeof(DrawData));
}