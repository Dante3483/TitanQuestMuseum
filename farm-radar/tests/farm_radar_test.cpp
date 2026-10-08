#include "backend/farm_radar_rules.h"
#include "ui/farm_radar_panel.h"
#include <cstdio>
#include <cwchar>
#include <fstream>
#include <limits>
#include <sstream>
#include <set>
namespace {
int failures=0,checks=0;
void check(bool ok,const char* what){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",what);}}
struct Recorder: museum::ui::Renderer {
    struct Text {museum::ui::Rect rect;std::wstring text;int size;};
    std::vector<Text> captions;std::vector<museum::ui::Rect> fills;
    void fill(museum::ui::Rect r,museum::ui::Color) override{fills.push_back(r);}
    float measure(const wchar_t* text,int size) override{return float(std::wcslen(text))*size*0.6f;}
    void text(museum::ui::Rect r,const wchar_t* text,int size,museum::ui::Color,museum::ui::TextAlign) override{
        captions.push_back({r,text,size});
    }
};

}
int main(){
    using namespace museum;using namespace museum::ui;
    std::map<std::string,double> nearest;
    radarRemember(nearest,"Satyr",900,30);
    check(nearest.count("Satyr")==1,"creature exactly on radius boundary is included");
    radarRemember(nearest,"Satyr",25,30);
    radarRemember(nearest,"Outside",900.01,30);radarRemember(nearest,"Dead/unreadable",-1,30);
    radarRemember(nearest,"Invalid",std::numeric_limits<double>::quiet_NaN(),30);
    radarRemember(nearest,"Boss",4,30);
    auto sorted=radarSorted(nearest);
    check(sorted.size()==2 && sorted.front().name=="Boss" && sorted.back().distanceSquared==25,
          "inclusive radius, unique names, nearest instance and nearest-first order");
    check(radarDistanceSquared({0,0,0,1},{0,0,0,2})<0,"different worlds never match");
    check(radarDistanceSquared({1000,0,1000,1},{1030,0,1000,1})==900,"world positions across region edges use real radius");
    for(const auto resolution:{std::pair<int,int>{640,480},{1920,1080},{3840,2160}}){
        Recorder r;FarmRadarPanel panel;std::vector<RadarEntry> names;
        for(int i=0;i<20;++i)names.push_back({i==0?std::string(160,'W'):"Creature "+std::to_string(i),double(i)});
        panel.draw(r,resolution.first,resolution.second,names);const Rect b=panel.bounds();
        check(b.x>=0&&b.y>=0&&b.right()<=resolution.first&&b.bottom()<=resolution.second,"panel stays on canvas");
        check(!r.captions.empty() && r.captions.front().text.back()==L'\u2026',"long names ellipsized");
        bool fits=true;for(const auto& caption:r.captions)
            if(r.measure(caption.text.c_str(),caption.size)>caption.rect.w || caption.rect.bottom()>b.bottom())fits=false;
        check(fits,"localized names stay within panel");
        check(!panel.scroll(b.right()+1,b.y,1,names.size()),"wheel outside panel not claimed");
        check(panel.scroll(b.x+5,b.y+5,100,names.size()) && panel.offset()==20-panel.visibleRows(),"wheel reaches last names without overflow");
        r.captions.clear();panel.draw(r,resolution.first,resolution.second,{});
        check(r.captions.empty()&&panel.offset()==0,"empty results retain blank panel and clear scroll");
    }
    std::printf("Farm radar: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
