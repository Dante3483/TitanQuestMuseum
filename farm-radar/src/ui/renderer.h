#pragma once
#include "ui/geometry.h"
namespace museum::ui {
struct Color { float r, g, b, a; };
enum class TextAlign { Left, Center, Right };
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual void fill(Rect rect, Color color) = 0;
    virtual float measure(const wchar_t* text, int size) = 0;
    virtual void text(Rect rect, const wchar_t* text, int size, Color color, TextAlign align) = 0;
    void outline(Rect r, Color c, float thickness = 1);
};
class TextRenderer {
public:
    explicit TextRenderer(Renderer& r) : renderer_(r) {}
    int fit(const wchar_t* text, int baseSize, int minimum, float width);
    void drawCentered(Rect r, const wchar_t* text, int size, Color color);
    void drawRight(Rect r, const wchar_t* text, int size, Color color);
private:
    Renderer& renderer_;
};
void widen(const char* ascii, wchar_t* output, int capacity);
}
