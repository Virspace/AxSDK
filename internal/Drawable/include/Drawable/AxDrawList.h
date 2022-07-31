#pragma once

#include "Foundation/AxTypes.h"

struct AxDrawCommand;
typedef uint32_t AxDrawIndex;

// TODO(mdeforge): Consider making this an opaque data structure so the user can define their own.
struct AxDrawVert
{
    AxVert Position;
    AxUV UV;
    uint32_t Color; // RGBA, 4 bytes each
};

// TODO(mdeforge): Rename buffer to something else, Array?
// A low-level list of polygons. At the end of the frame, all command lists are rendered.
struct AxDrawList
{
    // What we have to render
    struct AxDrawCommand *CommandBuffer;          // Draw commands
    struct AxDrawVert    *VertexBuffer;           // Vertex Buffer
    AxDrawIndex   *IndexBuffer;            // Index Buffer
    bool          IsDirty;                 // Triggers upload of the buffers to GPU
};

void DrawListAddDrawable(struct AxDrawList *DrawList, const struct AxDrawVert *Vertices, const AxDrawIndex *Indices, const size_t VertexCount, const size_t IndexCount);
void DrawListAddQuad(struct AxDrawList *DrawList, const AxVert TopRight, const AxVert BottomRight, const AxVert BottomLeft, const AxVert TopLeft);
void DrawListAddRect(struct AxDrawList* DrawList, const AxRect *Rect);
void DrawListClear(struct AxDrawList *DrawList);