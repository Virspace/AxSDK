#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "Foundation/AxTypes.h"



void DrawListAddDrawable(struct AxDrawList *DrawList, const struct AxDrawable Drawable);
void DrawListAddQuad(struct AxDrawList *DrawList, const AxVert TopRight, const AxVert BottomRight, const AxVert BottomLeft, const AxVert TopLeft);
void DrawListAddRect(struct AxDrawList* DrawList, const AxRect *Rect);
void DrawListClear(struct AxDrawList *DrawList);

void DrawDataAddDrawList(struct AxDrawData *DrawData, const struct AxDrawList DrawList);
void DrawDataClear(struct AxDrawData *DrawData);

#ifdef __cplusplus
}
#endif
