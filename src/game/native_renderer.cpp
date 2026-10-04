#include "game/native_renderer.h"
#include "core/logging.h"
#include <cmath>
#include <cstring>
#include <cwchar>

namespace museum::game {
namespace {
using namespace integration;
// The exported rectangle overload takes Rect BY VALUE (16 bytes), then 13 32-bit arguments.
// Engine.dll RenderText(Rect,wchar_t) inspected locally: see research/TEXT_RENDERING.md.
using RectText = float(__thiscall*)(TqCanvas*,TqRect,const TqColor*,const wchar_t*,const void*,
                                  int,int,int,Bool32,int,int,Bool32,int,Bool32,Bool32);
RectText rectText = nullptr;
bool resolved = false;
constexpr char rectTextName[] = "?RenderText@GraphicsCanvas@GAME@@QAEMVRect@2@ABVColor@2@PBGPBVGraphicsFont@2@HW4XAlignment@12@W4YAlignment@12@_NHW4RenderFontStyle@2@66H6@Z";
// Capability proof, derived from the exported wrapper's CALL, never a fixed code RVA.
bool alignmentVerified(const unsigned char* wrapper) {
    if (!readable(wrapper,256)) return false;
    for (int i=0;i<180;++i) {
        if (wrapper[i]!=0xE8) continue;
        int displacement=0; std::memcpy(&displacement,wrapper+i+1,4);
        const unsigned char* target=wrapper+i+5+displacement;
        unsigned char code[0xA0];
        if (!safeRead(target,code,sizeof(code))) continue;
        const unsigned char first[]={0x8B,0x44,0x24,0x20,0xF3,0x0F,0x10,0x44,0x24,0x08};
        // first decrement -> right; second decrement -> center. Same sequence for Y.
        if (std::memcmp(code,first,sizeof(first))==0 && code[0x17]==0x48 && code[0x18]==0x74 &&
            code[0x1A]==0x48 && code[0x1B]==0x75 && code[0x63]==0x48 &&
            code[0x64]==0x74 && code[0x66]==0x48 && code[0x67]==0x75) return true;
    }
    return false;
}
struct Measurement { const void* font=nullptr; int size=0; wchar_t text[192]={}; float width=0; };
Measurement cache[128];
unsigned next=0;
}
bool nativeTextAvailable() {
    if (!resolved) {
        resolved=true;
        auto proc=GetProcAddress(GetModuleHandleW(L"Engine.dll"),rectTextName);
        if (proc && alignmentVerified(reinterpret_cast<const unsigned char*>(proc)))
            rectText=reinterpret_cast<RectText>(proc);
        integration::logI("museum: native rectangle text alignment %s (0 left, 1 right, 2 center)",
                         rectText ? "verified" : "unavailable; captions disabled");
    }
    return rectText != nullptr;
}
void forgetTextMeasurements() { for (auto& c:cache) c.font=nullptr; next=0; }
void NativeRenderer::fill(ui::Rect r,ui::Color c) {
    TqRect rect={r.x,r.y,r.w,r.h};
    TqColor color={c.r,c.g,c.b,c.a};
    integration::g_tq.CanvasRenderRect(canvas_,&rect,&color,nullptr,nullptr);
}
float NativeRenderer::measure(const wchar_t* text,int size) {
    if (!font_ || !nativeTextAvailable() || !integration::g_tq.CanvasRenderTextW) return 1e6f;
    for (const auto& c:cache)
        if (c.font==font_ && c.size==size && std::wcscmp(c.text,text)==0) return c.width;
    // Public RenderText returns the native glyph advance. Measure invisibly outside the canvas;
    // no guessed glyph widths or undocumented private font-object reads are used.
    const TqColor transparent={1,1,1,0};
    const float width=integration::g_tq.CanvasRenderTextW(canvas_,-30000,-30000,&transparent,text,
                                                        font_,size,0,0,0u,0,0,0);
    if (!std::isfinite(width) || width<0 || (*text && width==0)) return 1e6f;
    if (std::wcslen(text)<192) {
        auto& c=cache[next++%128]; c.font=font_; c.size=size;
        std::wcscpy(c.text,text); c.width=width;
    }
    return width;
}
void NativeRenderer::text(ui::Rect r,const wchar_t* text,int size,ui::Color c,ui::TextAlign align) {
    if (!font_ || !nativeTextAvailable() || !text || !*text) return;
    const TqRect rect={r.x,r.y,r.w,r.h};
    const TqColor color={c.r,c.g,c.b,c.a};
    const int x=align==ui::TextAlign::Center ? 2 : align==ui::TextAlign::Right ? 1 : 0;
    rectText(canvas_,rect,&color,text,font_,size,x,2,0u,0,0,0u,0,0u,0u);
}
}
