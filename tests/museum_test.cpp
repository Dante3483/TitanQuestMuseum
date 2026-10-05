#include "backend/localization.h"
#include "backend/museum_state.h"
#include "backend/model/layout.h"
#include "game/view_safety.h"
#include "game/caravan_geometry.h"
#include "ui/museum_panel.h"
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <vector>
using namespace museum;
using namespace museum::ui;
namespace {
int failures=0, checks=0;
void check(bool value,const char* message) {
    ++checks;
    if (!value) { ++failures; std::printf("FAIL: %s\n",message); }
}
bool near(float a,float b) { return std::fabs(a-b)<0.01f; }
struct Draw { Rect rect; std::wstring text; int font; TextAlign alignment; };
class Recorder final : public Renderer {
public:
    std::vector<Draw> draws;
    void fill(Rect,Color) override {}
    // Deliberately non-uniform test font, not used by the game.
    float measure(const wchar_t* s,int size) override {
        float sum=0; for (; *s; ++s) sum+=*s==L'i' || *s==L'l' ? 0.25f : *s==L'W' ? 0.9f : 0.55f;
        return sum*size;
    }
    void text(Rect r,const wchar_t* s,int font,Color,TextAlign alignment) override {
        draws.push_back({r,s,font,alignment});
    }
};
MuseumState fixture(Section section) {
    CollectionSnapshot source;
    source.mode=Mode::Collection; source.searchEnabled=true;
    const char* labels[]={"Helms","Torso","Arms","Legs","Amulets","Rings",
        "Swords","Axes","Maces","Spears","Bows","Throwing","Staves","Shields","Artifacts"};
    source.categoryCount=15;
    for (int i=0;i<15;++i) source.categories[i]={i,categorySection(labels[i]),categoryCaption(labels[i]),100,false,true};
    source.selectedCategory=source.shownCategory=section==Section::Equipment ? 1 : section==Section::Weapons ? 12 : 14;
    source.categoryCounts={12,462,true}; source.allCounts={241,2688,true};
    return makeMuseumState(source);
}
}
int main() {
    {
        wchar_t decoded[32];
        widen("Поиск",decoded,32);
        check(std::wcscmp(decoded,L"Поиск")==0,"UTF-8 Cyrillic captions decode correctly");
        i18n::load(".","RU");
        check(std::strcmp(sectionCaption(Section::Weapons),"Оружие")==0,"Russian section resource loads");
        i18n::load(".","EN");
        check(std::strcmp(sectionCaption(Section::Weapons),"Weapons")==0,"English resource restores captions");
    }

    {
        CollectionSnapshot source=fixture(Section::Equipment);
        source.searchText[0]=L'a';
        source.categories[0].searchMatch=true;
        source.categories[6].searchMatch=true;
        auto state=makeMuseumState(source);
        check(state.sectionSearchMatch[0] && state.sectionSearchMatch[1] && !state.sectionSearchMatch[2],
              "search marks all sections containing matching categories");
        source.searchText[0]=0;
        state=makeMuseumState(source);
        check(!state.sectionSearchMatch[0] && !state.sectionSearchMatch[1] && !state.sectionSearchMatch[2],
              "empty search clears section marks");
    }

    for (int footprint=1; footprint<=5; ++footprint) {
        const gdut::SlotGeometry g={1,footprint,16,15/footprint};
        int end=0;
        for (int row=0; row<g.rows; ++row) {
            gdut::PackPlacement p; int height=0;
            check(gdut::placeInSlot(g,row*g.cols,0,1,footprint,&p),"slot placement before height distribution");
            gdut::fillWindowSlot(g,15,footprint,&p,&height);
            check(p.slotRow==end && height>=footprint && p.row>=p.slotRow && p.row+footprint<=p.slotRow+height,"distributed slots have no gaps and contain original footprint");
            end=p.slotRow+height;
        }
        check(end==15,"every full category window occupies all fifteen host rows");
        check(gdut::filledRowEdge(g,g.rows*g.h,15)==15,"unused tail cover has zero height after distribution");
    }
    for (float scale:{0.5f,0.8f,1.0f,1.25f,1.5f,2.0f}) {
        const Rect frame={66.0f*scale,66.0f*scale,565.0f*scale,637.0f*scale};
        for (Section section:{Section::Equipment,Section::Weapons,Section::Other}) {
            auto state=fixture(section); MuseumPanel panel; panel.arrange(frame,scale,state);
            const auto& l=panel.layout();
            check(panel.visible(),"panel visible at every supported UI scale");
            check(near(l.panel.y,pixel(frame.bottom())),"panel begins below entire caravan");
            check(near(l.panel.x,pixel(frame.x)) && near(l.panel.right(),pixel(frame.right())),"panel aligns with both caravan frame edges");
            check(near(l.search.w,l.transfer.w),"Search width equals Transfer width");
            check(l.statistics.x>l.owned.right(),"statistics has its own area after Owned");
            check(near(l.statistics.right(),l.sections.right()),"statistics ends at fixed inner edge");
            check(near(l.categories.w,l.sections.w),"categories use full row width");
            check(near(l.transfer.w,l.collection.w) || std::fabs(l.transfer.w-l.collection.w)<=1,
                  "Transfer and Collection split width with at most one rounding pixel");
            const int count=state.sectionCategoryCount;
            check(count==(section==Section::Equipment ? 6 : section==Section::Weapons ? 8 : 1),"dynamic section categories");
            for (int i=0;i<count;++i) {
                const Rect r=rowCell(l.categories,count,i,pixel(4*scale));
                const int id=panel.hit(r.x+r.w/2,r.y+r.h/2);
                check(panel.action(id).kind==ActionKind::SelectCategory && panel.action(id).value==state.sectionCategories[i].id,
                      "drawn category rectangle is the input target");
                if (i==count-1) check(near(r.right(),l.categories.right()),"last category reaches right edge");
                if (count==1) check(near(r.w,l.categories.w),"Artifact fills complete row");
            }
            check(panel.action(panel.hit(l.transfer.x+1,l.transfer.y+1)).kind==ActionKind::ShowTransfer,"permanent Transfer button");
            check(panel.action(panel.hit(l.collection.x+1,l.collection.y+1)).kind==ActionKind::ShowCollection,"permanent Collection button");
            Recorder renderer; panel.draw(renderer,state,-1,-1,0);
            for (const auto& d:renderer.draws) {
                if (d.text==L"Search") check(d.alignment==TextAlign::Left && near(d.rect.x,l.search.x+pixel(3*scale)),"empty Search uses a small left inset");
                check(d.text!=L"<" && d.text!=L">" && d.text.find(L"page")==std::wstring::npos,"no visible pagination labels");
                if (d.text==L"Owned" || d.text==L"Transfer" || d.text==L"Collection" || d.text==L"Equipment")
                    check(d.font==static_cast<int>(pixel(13*scale)),"stable general font across sections");
                if (d.text.find(L"All 241/2688")!=std::wstring::npos) {
                    check(d.alignment==TextAlign::Right,"statistics uses native right alignment");
                    check(near(d.rect.right(),l.statistics.right()),"statistics final anchor is fixed");
                    check(renderer.measure(d.text.c_str(),d.font)<=d.rect.w,"statistics fits without overlapping Owned");
                }
            }
        }
    }
    // Statistics changes content and font locally, never its anchor or another control's font.
    Recorder r; StatisticsView view{{100,10,140,30}};
    for (const char* name:{"Torso","Legs","Staff","Spear","Amulet"}) {
        auto state=fixture(Section::Equipment); state.shownCategoryName=name;
        view.draw(r,state,13,6,3);
        check(!r.draws.empty() && near(r.draws.back().rect.right(),240),"changing category names preserves right anchor");
    }
    // Actual preserved slot-window functions, not a reimplementation of the wheel policy.
    gdut::SlotGeometry slots; std::vector<gdut::PackItem> items;
    for (int i=0;i<100;++i) items.push_back({i,2,3});
    check(gdut::slotOf(items,16,15,&slots),"original slot footprint geometry");
    check(gdut::maxRowOffset(slots,100)==8,"last window clamp preserves original packing");
    check(gdut::clampRowOffset(slots,100,99)==8,"wheel cannot go beyond last full window");
    integration::UtWheelAcc wheel;
    check(integration::utWheelAccumulate(wheel,60)==0 && integration::utWheelAccumulate(wheel,60)==1,"partial wheel notches accumulate");
    integration::UtViewState vs;
    vs.bindingsOk=true;
    integration::utViewApply(&vs,integration::kUtViewEvWorldUp,0);
    integration::utViewApply(&vs,integration::kUtViewEvOpen,0);
    integration::utViewApply(&vs,integration::kUtViewEvModeChanged,1);
    integration::utViewApply(&vs,integration::kUtViewEvToggle,0);
    check(vs.on,"view enables only on live Transfer tab");
    integration::utViewApply(&vs,integration::kUtViewEvGoodbye,0);
    check(!vs.on,"closing caravan restores native view");
    std::printf("Museum: %d checks, %d failures\n",checks,failures);
    return failures ? 1 : 0;
}
