#pragma once
#include "backend/collection_runtime.h"
#include "game/caravan_geometry.h"
namespace integration {
inline UtRectF visualSlotRect(const UtRectF& grid,int col,int row,int w,int h) {
    UtRectF r=utSlotDrawnRect(grid,col,row,w,h);
    const float cw=grid.w/16.0f,ch=grid.h/15.0f;
    r.x=grid.x+liveVisualColumn((float)col)*cw;
    r.w=(liveVisualColumn((float)(col+w))-liveVisualColumn((float)col))*cw;
    r.y=grid.y+liveVisualCell((float)row)*ch;
    r.h=(liveVisualCell((float)(row+h))-liveVisualCell((float)row))*ch;
    return r;
}
}
