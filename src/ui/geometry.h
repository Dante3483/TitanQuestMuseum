#pragma once
#include <cmath>
namespace museum::ui {
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    float right() const { return x + w; }
    float bottom() const { return y + h; }
    bool contains(float px, float py) const {
        return w > 0 && h > 0 && px >= x && px < right() && py >= y && py < bottom();
    }
    Rect inset(float p) const { return {x+p, y+p, w-2*p, h-2*p}; }
};
inline float pixel(float v) { return std::floor(v + 0.5f); }
// Round shared edges, not widths: the last control always ends at the row's right edge.
inline Rect rowCell(Rect row, int count, int index, float gap) {
    if (count < 1 || index < 0 || index >= count || row.w <= gap*(count-1)) return {};
    const float width = (row.w-gap*(count-1))/count;
    const float left = pixel(row.x+index*(width+gap));
    const float right = index == count-1 ? pixel(row.right()) : pixel(row.x+index*(width+gap)+width);
    return {left, pixel(row.y), right-left, pixel(row.h)};
}
struct MuseumStyle {
    float outerMargin = 0, padding = 6, rowHeight = 22, rowGap = 4, controlGap = 4;
    float ownedWidth = 60, textPadding = 3;
    int fontSize = 13, statsMinFontSize = 6;
};
struct PanelLayout {
    Rect panel, sections, categories, search, clearSearch, searchText, owned, statistics, transfer, collection;
};
PanelLayout layoutPanel(Rect caravan, float scale, const MuseumStyle& style);
}
