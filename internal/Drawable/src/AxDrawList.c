#pragma once

#include "AxDrawList.h"
#include "AxDrawCommand.h"
#include <string.h>
#include "Foundation/AxArray.h"

void DrawListAddDrawable(struct AxDrawList *DrawList, const struct AxDrawVert *Vertices, const AxDrawIndex *Indices, const size_t VertexCount, const size_t IndexCount)
{
    Assert(DrawList);

    struct AxDrawCommand DrawCommand = {
        .TextureID = 0,
        .VertexOffset = ArraySize(DrawList->VertexBuffer), // Size of the buffer up until this point
        .IndexOffset = ArraySize(DrawList->IndexBuffer),   // Size of the buffer up until this point
        .ElementCount = ArraySize(Indices)
    };

    ArrayPush(DrawList->CommandBuffer, DrawCommand);
    ArrayPushArray(DrawList->VertexBuffer, Vertices);
    ArrayPushArray(DrawList->IndexBuffer, Indices);
    DrawList->IsDirty = true;
}

void DrawListAddQuad(struct AxDrawList *DrawList, const AxVert TopRight, const AxVert BottomRight, const AxVert BottomLeft, const AxVert TopLeft)
{
    Assert(DrawList);

    struct AxDrawVert *Vertices = NULL;
    AxDrawIndex *Indices = NULL;
    AxUV UV1 = (AxUV) { .U = 0, .V = 0 };

    ArrayPush(Vertices, ((struct AxDrawVert){ .Position = TopRight, .UV = UV1, .Color = 0 }));
    ArrayPush(Vertices, ((struct AxDrawVert){ .Position = BottomRight, .UV = UV1, .Color = 0 }));
    ArrayPush(Vertices, ((struct AxDrawVert){ .Position = BottomLeft, .UV = UV1, .Color = 0 }));
    ArrayPush(Vertices, ((struct AxDrawVert){ .Position = TopLeft, .UV = UV1, .Color = 0 }));

    ArrayPush(Indices, 0);
    ArrayPush(Indices, 1);
    ArrayPush(Indices, 3);
    ArrayPush(Indices, 1);
    ArrayPush(Indices, 2);
    ArrayPush(Indices, 3);

    DrawListAddDrawable(DrawList, Vertices, Indices, ArraySize(Vertices), ArraySize(Indices));

    ArrayFree(Vertices);
    ArrayFree(Indices);
}

void DrawListAddRect(struct AxDrawList* DrawList, const AxRect *Rect)
{
    const AxVert TopRight = { (float)Rect->Right, (float)Rect->Top, 0.0f };
    const AxVert BottomRight = { (float)Rect->Right, (float)Rect->Bottom, 0.0f };
    const AxVert BottomLeft = { (float)Rect->Left, (float)Rect->Bottom, 0.0f };
    const AxVert TopLeft = { (float)Rect->Left, (float)Rect->Top, 0.0f };

    DrawListAddQuad(DrawList, TopRight, BottomRight, BottomLeft, TopLeft);
}

void DrawListClear(struct AxDrawList *DrawList)
{
    Assert(DrawList);

    ArrayFree(DrawList->CommandBuffer);
    ArrayFree(DrawList->VertexBuffer);
    ArrayFree(DrawList->IndexBuffer);
}