#include "ui/farm_radar_panel.h"

#include <cwchar>
namespace museum::ui {
void FarmRadarPanel::draw(Renderer& r,int width,int height,const std::vector<RadarEntry>& rows) {
    shown_=false;
    if(width<320 || height<240)return;
    const float scale=(std::max)(0.75f,(std::min)(1.5f,height/900.0f));
    const float margin=18*scale,padding=14*scale,rowHeight=25*scale;
    visible_=(std::min)(8,(std::max)(1,int((height*0.28f-2*padding)/rowHeight)));
    offset_=radarClampOffset(offset_,int(rows.size()),visible_);
    const int count=(std::min)(visible_,int(rows.size())-offset_);
    const float panelHeight=(std::max)(44*scale,2*padding+count*rowHeight);
    bounds_={margin,height-margin-panelHeight,(std::min)(304*scale,width-2*margin),panelHeight};
    shown_=true;
    const Rect b=bounds_;
    r.fill({b.x+4*scale,b.y+5*scale,b.w,b.h},{0,0,0,0.45f});
    r.fill(b,{0.055f,0.061f,0.058f,0.94f});
    r.fill({b.x+scale,b.y+scale,b.w-2*scale,b.h*0.45f},{0.11f,0.105f,0.077f,0.82f});
    r.outline(b,{0.39f,0.33f,0.19f,0.95f},scale);
    r.fill({b.x,b.y,2*scale,b.h},{0.76f,0.61f,0.28f,1});
    // Restrained brass corner details; the content contains creature names only.
    r.fill({b.x,b.y,24*scale,scale},{0.86f,0.73f,0.43f,1});
    r.fill({b.right()-24*scale,b.bottom()-scale,24*scale,scale},{0.67f,0.55f,0.29f,1});
    const int font=(std::max)(10,int(15*scale));
    const float textWidth=b.w-2*padding-8*scale;
    for(int i=0;i<count;++i){
        wchar_t buffer[1024];widen(rows[size_t(offset_+i)].name.c_str(),buffer,1024);
        std::wstring text=buffer;
        if(r.measure(text.c_str(),font)>textWidth){
            while(!text.empty() && r.measure((text+L"\u2026").c_str(),font)>textWidth){
                text.pop_back();
                if(!text.empty() && text.back()>=0xD800 && text.back()<=0xDBFF)text.pop_back();
            }
            text+=L"\u2026";
        }
        const float top=b.y+padding+i*rowHeight;
        r.fill({b.x+padding-3*scale,top+rowHeight*0.46f,3*scale,3*scale},{0.76f,0.61f,0.28f,1});
        r.text({b.x+padding+5*scale,top,textWidth,rowHeight},text.c_str(),font,
               {0.93f,0.87f,0.71f,1},TextAlign::Left);
    }
    if(int(rows.size())>visible_){
        const float rail=b.h-2*padding;
        const float thumb=rail*visible_/int(rows.size());
        const float top=b.y+padding+(rail-thumb)*offset_/(int(rows.size())-visible_);
        r.fill({b.right()-6*scale,top,2*scale,thumb},{0.60f,0.50f,0.28f,0.9f});
    }
}
bool FarmRadarPanel::scroll(float x,float y,int steps,size_t total) {
    if(!shown_ || !bounds_.contains(x,y) || int(total)<=visible_)return false;
    offset_=radarClampOffset(offset_+steps,int(total),visible_);return true;
}
}
