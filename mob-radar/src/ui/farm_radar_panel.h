#pragma once
#include "backend/farm_radar_rules.h"
#include "ui/renderer.h"
namespace museum::ui {
class FarmRadarPanel {
public:
    void draw(Renderer&,int canvasWidth,int canvasHeight,const std::vector<RadarEntry>&);
    bool scroll(float x,float y,int steps,size_t total);
    void reset(){offset_=0;shown_=false;labels_.clear();}
    Rect bounds() const{return bounds_;}
    int visibleRows() const{return visible_;}
    int offset() const{return offset_;}
private:
    Rect bounds_={};int visible_=0,offset_=0;bool shown_=false;
    struct Label {std::string name,item;std::wstring text,itemText;int font=0;float width=0;};
    std::vector<Label> labels_;
};
}
