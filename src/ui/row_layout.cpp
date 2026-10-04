#include "ui/geometry.h"
namespace museum::ui {
PanelLayout layoutPanel(Rect caravan, float scale, const MuseumStyle& style) {
    PanelLayout l;
    if (!(scale > 0.3f && scale < 4.0f) || !(caravan.w > 0 && caravan.h > 0)) return l;
    const float margin = pixel(style.outerMargin*scale), p = pixel(style.padding*scale);
    const float h = pixel(style.rowHeight*scale), rg = pixel(style.rowGap*scale);
    const float gap = pixel(style.controlGap*scale);
    l.panel = {pixel(caravan.x+margin), pixel(caravan.bottom()), pixel(caravan.w-2*margin), 2*p+4*h+3*rg};
    const Rect inner = l.panel.inset(p);
    l.sections = {inner.x,inner.y,inner.w,h};
    l.categories = {inner.x,inner.y+h+rg,inner.w,h};
    const Rect bottom = {inner.x,inner.y+3*(h+rg),inner.w,h};
    l.transfer = rowCell(bottom,2,0,gap); l.collection = rowCell(bottom,2,1,gap);
    l.search = {inner.x,inner.y+2*(h+rg),l.transfer.w,h};
    l.owned = {l.search.right()+gap,l.search.y,pixel(style.ownedWidth*scale),h};
    l.statistics = {l.owned.right()+gap,l.search.y,inner.right()-l.owned.right()-gap,h};
    l.clearSearch = {l.search.right()-h,l.search.y,h,h};
    l.searchText = {l.search.x,l.search.y,l.search.w-h-gap,h};
    return l;
}
}
