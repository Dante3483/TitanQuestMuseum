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
    int i=0;
    const unsigned char* p=reinterpret_cast<const unsigned char*>(ascii);
    while(p && *p && i<capacity-1) {
        unsigned cp=*p++; int remaining=0;
        if(cp>=0xF0 && cp<=0xF4) {cp&=7;remaining=3;}
        else if(cp>=0xE0 && cp<=0xEF) {cp&=15;remaining=2;}
        else if(cp>=0xC2 && cp<=0xDF) {cp&=31;remaining=1;}
        else if(cp>=0x80) cp=0xFFFD;
        bool valid=true;
        for(int n=0;n<remaining;++n) {
            if(!*p || (*p&0xC0)!=0x80) {valid=false;break;}
            cp=(cp<<6)|(*p++&63);
        }
        if(!valid || cp>0x10FFFF || (cp>=0xD800 && cp<=0xDFFF) ||
           (remaining==1 && cp<0x80) || (remaining==2 && cp<0x800) || (remaining==3 && cp<0x10000)) cp=0xFFFD;
        if(cp>0xFFFF && sizeof(wchar_t)==2) {
            if(i+2>=capacity) break;
            cp-=0x10000;out[i++]=static_cast<wchar_t>(0xD800+(cp>>10));
            out[i++]=static_cast<wchar_t>(0xDC00+(cp&1023));
        } else out[i++]=static_cast<wchar_t>(cp);
    }
    out[i]=0;
}
}
