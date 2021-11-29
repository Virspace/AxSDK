#pragma once

#include "Foundation/Types.h"

typedef struct AxDrawCommand AxDrawCommand;
typedef uint32_t AxDrawIndex;

typedef struct AxDrawVert
{
    AxVert Position;
    AxUV UV;
    uint32_t Color; // TODO(mdeforge): Store as hex, or something? Provide convert functions?
} AxDrawVert;

// TODO(mdeforge): Rename buffer to something else, Array?
// A low-level list of polygons. At the end of the frame, all command lists are rendered.
typedef struct AxDrawList
{
    // What we have to render
    AxDrawCommand *CommandBuffer;          // Draw commands
    AxDrawVert    *VertexBuffer;           // Vertex Buffer
    AxDrawIndex   *IndexBuffer;            // Index Buffer
    bool          IsDirty;                 // Triggers upload of the buffers to GPU
} AxDrawList;

void DrawListAddDrawable(AxDrawList *DrawList, const AxDrawVert *Vertices, const AxDrawIndex *Indices, const size_t VertexCount, const size_t IndexCount);
void DrawListAddQuad(AxDrawList *DrawList, const AxVert TopRight, const AxVert BottomRight, const AxVert BottomLeft, const AxVert TopLeft);
void DrawListAddRect(AxDrawList* DrawList, const AxRect *Rect);
void DrawListClear(AxDrawList *DrawList);