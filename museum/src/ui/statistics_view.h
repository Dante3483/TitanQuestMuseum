#pragma once
#include "ui/renderer.h"
#include "backend/museum_state.h"
namespace museum::ui {
struct StatisticsView {
    Rect rect;
    void draw(Renderer& renderer, const MuseumState& state, int baseFont, int minFont, float padding) const;
};
}
