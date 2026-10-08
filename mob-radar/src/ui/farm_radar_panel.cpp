#include "ui/farm_radar_panel.h"

#include <cwchar>
namespace museum::ui {
namespace {
std::wstring fitted(Renderer& r,const std::string& value,int font,float width){
    wchar_t buffer[1024];widen(value.c_str(),buffer,1024);std::wstring text=buffer;
    if(r.measure(text.c_str(),font)>width){
        while(!text.empty()&&r.measure((text+L"\u2026").c_str(),font)>width){
            text.pop_back();if(!text.empty()&&text.back()>=0xD800&&text.back()<=0xDBFF)text.pop_back();
        }
        text+=L"\u2026";
    }
    return text;
}
}

void FarmRadarPanel::draw(Renderer& r,int width,int height,const std::vector<RadarEntry>& rows) {
    shown_=false;
    if(width<320 || height<240)return;
    const float scale=(std::max)(0.75f,(std::min)(1.5f,height/900.0f));
    const bool hasItems=std::any_of(rows.begin(),rows.end(),[](const auto& row){return !row.item.empty();});
    const float margin=18*scale,padding=14*scale,rowHeight=(hasItems?44:25)*scale;
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
    // Restrained brass corner details around names and optional Museum loot captions.
    r.fill({b.x,b.y,24*scale,scale},{0.86f,0.73f,0.43f,1});
    r.fill({b.right()-24*scale,b.bottom()-scale,24*scale,scale},{0.67f,0.55f,0.29f,1});
    const int font=(std::max)(10,int(15*scale));
    const float textWidth=b.w-2*padding-8*scale;
    labels_.resize(size_t(count));
    for(int i=0;i<count;++i){
        auto& label=labels_[size_t(i)];const auto& entry=rows[size_t(offset_+i)];
        const int itemFont=(std::max)(10,int(12*scale));
        if(label.name!=entry.name||label.item!=entry.item||label.font!=font||label.width!=textWidth){
            label.name=entry.name;label.item=entry.item;label.font=font;label.width=textWidth;
            label.text=fitted(r,entry.name,font,textWidth);
            label.itemText=entry.item.empty()?std::wstring():fitted(r,entry.item,itemFont,textWidth);
        }
        const float top=b.y+padding+i*rowHeight;
        r.fill({b.x+padding-3*scale,top+12*scale,3*scale,3*scale},{0.76f,0.61f,0.28f,1});
        r.text({b.x+padding+5*scale,top,textWidth,25*scale},label.text.c_str(),font,
               rows[size_t(offset_+i)].highlighted?Color{1.0f,0.77f,0.27f,1}:Color{0.93f,0.87f,0.71f,1},TextAlign::Left);
        if(!entry.item.empty())r.text({b.x+padding+5*scale,top+23*scale,textWidth,21*scale},label.itemText.c_str(),itemFont,
            {1.0f,0.83f,0.45f,1},TextAlign::Left);
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
