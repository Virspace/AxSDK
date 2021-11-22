#pragma once

typedef struct AxDrawCommand
{
    int32_t TextureID;
    size_t VertexOffset; // Start offset in the vertex buffer
    size_t IndexOffset;  // Start offset in the index buffer
    size_t ElementCount; // Number of indices to be rendered as triangles
} AxDrawCommand;