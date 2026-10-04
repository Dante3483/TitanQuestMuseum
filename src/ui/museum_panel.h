#pragma once
#include "ui/button.h"
#include "ui/statistics_view.h"
namespace museum::ui {
class MuseumPanel {
public:
    void arrange(Rect caravan,float scale,const MuseumState& state);
    void hide();
    int hit(float x,float y) const;
    Action action(int control) const;
    bool isSearch(int control) const;
    bool switchesMode(int control) const;
    void draw(Renderer& renderer,const MuseumState& state,int hovered,int pressed,unsigned long ticks) const;
    const PanelLayout& layout() const { return layout_; }
    bool visible() const { return visible_; }
    bool searchShown() const { return searchShown_; }
private:
    MuseumStyle style_;
    PanelLayout layout_;
    Button buttons_[maxCategories+6];
    int buttonCount_ = 0;
    bool visible_ = false, searchShown_ = false;
    float scale_ = 1;
};
}
