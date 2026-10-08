#pragma once
#include "ui/renderer.h"
#include "game/game_api.h"
namespace museum::game {
class NativeRenderer final : public ui::Renderer {
public:
    NativeRenderer(TqCanvas* canvas,const void* font) : canvas_(canvas),font_(font) {}
    void fill(ui::Rect rect,ui::Color color) override;
    float measure(const wchar_t* text,int size) override;
    void text(ui::Rect rect,const wchar_t* text,int size,ui::Color color,ui::TextAlign align) override;
private:
    TqCanvas* canvas_;
    const void* font_;
};
bool nativeTextAvailable();
void forgetTextMeasurements();
}
