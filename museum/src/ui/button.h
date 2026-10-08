#pragma once
#include "ui/renderer.h"
#include "backend/museum_state.h"
namespace museum::ui {
inline constexpr Color ground = {0.302f,0.243f,0.145f,1};
inline constexpr Color normal = {0.380f,0.275f,0.129f,1};
inline constexpr Color hover = {0.475f,0.424f,0.255f,1};
inline constexpr Color selected = {0.667f,0.584f,0.322f,1};
inline constexpr Color gold = {0.847f,0.776f,0.565f,1};
inline constexpr Color dark = {0.098f,0.086f,0.055f,1};
inline constexpr Color match = {0.227f,0.384f,0.533f,1};
struct Button {
    Rect rect;
    const char* caption = "";
    Action action;
    bool selected = false, enabled = true, marked = false, dimmed = false;
    bool hit(float x,float y) const { return rect.contains(x,y); }
    void draw(Renderer& renderer, int baseFont, float padding, bool hovered, bool pressed) const;
};
}
