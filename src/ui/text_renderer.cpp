#include "ui/renderer.h"
namespace museum::ui {
void Renderer::outline(Rect r, Color c, float t) {
    fill({r.x,r.y,r.w,t},c); fill({r.x,r.bottom()-t,r.w,t},c);
    fill({r.x,r.y,t,r.h},c); fill({r.right()-t,r.y,t,r.h},c);
}
int TextRenderer::fit(const wchar_t* text, int size, int minimum, float width) {
    for (; size >= minimum; --size) if (renderer_.measure(text,size) <= width) return size;
    return 0; // never overlap another component if the available area is unusable
}
void TextRenderer::drawCentered(Rect r,const wchar_t* text,int size,Color color) {
    renderer_.text(r,text,size,color,TextAlign::Center);
}
void TextRenderer::drawRight(Rect r,const wchar_t* text,int size,Color color) {
    renderer_.text(r,text,size,color,TextAlign::Right);
}
void widen(const char* ascii,wchar_t* out,int capacity) {
    if (capacity < 1) return;
    int i = 0;
    for (; ascii && ascii[i] && i < capacity-1; ++i) out[i] = static_cast<unsigned char>(ascii[i]);
    out[i] = 0;
}
}
