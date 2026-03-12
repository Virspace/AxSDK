#pragma once

#include "AxDrawable.h"
#include "Foundation/AxArray.h"
#include <string.h>

struct AxDrawable *DrawableCreate()
{

}

void DrawDataAddDrawList(struct AxDrawData *DrawData, const struct AxDrawList *DrawList)
{
    DrawData->TotalVertexCount += ArraySize(DrawList.VertexBuffer);
    DrawData->TotalIndexCount += ArraySize(DrawList.IndexBuffer);

    ArrayPush(DrawData->DrawLists, DrawList);
}

void DrawDataClear(struct AxDrawData *DrawData)
{
    for (int i = 0; i < ArraySize(DrawData->DrawLists); ++i) {
        DrawListClear(&DrawData->DrawLists[i]);
    }

    ArrayFree(DrawData->DrawLists);
    memset(&DrawData, 0, sizeof(DrawData));
}

void DrawListAddDrawable(struct AxDrawList *DrawList, const struct AxDrawable Drawable)
{
    Assert(DrawList);

    struct AxDrawCommand DrawCommand = {
        .TextureID = 0,
        .VertexOffset = ArraySize(DrawList->VertexBuffer), // Size of the buffer up until this point
        .IndexOffset = ArraySize(DrawList->IndexBuffer),   // Size of the buffer up until this point
        .ElementCount = ArraySize(Drawable.Indices)
    };

    ArrayPush(DrawList->CommandBuffer, DrawCommand);
    ArrayPushArray(DrawList->VertexBuffer, Drawable.Vertices);
    ArrayPushArray(DrawList->IndexBuffer, Drawable.Indices);
    DrawList->IsDirty = true;
}

void DrawListAddQuad(struct AxDrawList *DrawList, const AxVert TopRight, const AxVert BottomRight, const AxVert BottomLeft, const AxVert TopLeft)
{
    Assert(DrawList);

    struct AxVertex *Vertices = NULL;
    AxDrawIndex *Indices = NULL;
    AxUV UV1 = (AxUV) { .U = 0, .V = 0 };

    ArrayPush(Vertices, ((struct AxVertex){ .Position = TopRight, .TexCoord = UV1, .Color = 0 }));
    ArrayPush(Vertices, ((struct AxVertex){ .Position = BottomRight, .TexCoord = UV1, .Color = 0 }));
    ArrayPush(Vertices, ((struct AxVertex){ .Position = BottomLeft, .TexCoord = UV1, .Color = 0 }));
    ArrayPush(Vertices, ((struct AxVertex){ .Position = TopLeft, .TexCoord = UV1, .Color = 0 }));

    ArrayPush(Indices, 0);
    ArrayPush(Indices, 1);
    ArrayPush(Indices, 3);
    ArrayPush(Indices, 1);
    ArrayPush(Indices, 2);
    ArrayPush(Indices, 3);

    const struct AxDrawable Drawable = {
        .Vertices = Vertices,
        .Indices = Indices,
        .VertexCount = ArraySize(Vertices),
        .IndexCount = ArraySize(Indices)
    };

    DrawListAddDrawable(DrawList, Drawable);

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