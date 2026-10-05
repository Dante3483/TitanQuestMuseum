#include "ui/museum_panel.h"
#include <cwchar>
namespace museum::ui {
namespace { constexpr int searchId = 1000, clearId = 1001; }
void MuseumPanel::hide() { visible_=false; buttonCount_=0; searchShown_=false; }
void MuseumPanel::arrange(Rect caravan,float scale,const MuseumState& state) {
    hide(); scale_=scale; layout_=layoutPanel(caravan,scale,style_);
    if (layout_.statistics.w <= 2*style_.textPadding*scale || layout_.panel.w <= 0) return;
    visible_=true;
    searchShown_ = state.searchEnabled;
    const float gap = pixel(style_.controlGap*scale);
    for (int i=0;i<3;++i) {
        const Section section=static_cast<Section>(i);
        buttons_[buttonCount_++]={rowCell(layout_.sections,3,i,gap),sectionCaption(section),
            {ActionKind::SelectSection,i},state.activeSection==section,true,state.sectionSearchMatch[i]};
    }
    for (int i=0;i<state.sectionCategoryCount;++i) {
        const auto& c=state.sectionCategories[i];
        buttons_[buttonCount_++]={rowCell(layout_.categories,state.sectionCategoryCount,i,gap),
            c.caption,{ActionKind::SelectCategory,c.id},state.selectedCategory==c.id,true,
            c.searchMatch,state.searchText[0]!=0 && c.indexed && !c.searchMatch};
    }
    buttons_[buttonCount_++]={layout_.owned,"Owned",{ActionKind::ToggleOwned,0},state.ownedOnly};
    buttons_[buttonCount_++]={layout_.transfer,"Transfer",{ActionKind::ShowTransfer,0},state.mode==Mode::Transfer};
    buttons_[buttonCount_++]={layout_.collection,"Collection",{ActionKind::ShowCollection,0},state.mode==Mode::Collection};
}
int MuseumPanel::hit(float x,float y) const {
    if (!visible_) return -1;
    if (layout_.clearSearch.contains(x,y)) return clearId;
    if (layout_.search.contains(x,y)) return searchId;
    for (int i=0;i<buttonCount_;++i) if (buttons_[i].hit(x,y)) return i;
    return -1;
}
Action MuseumPanel::action(int i) const {
    if (i==searchId) return {ActionKind::FocusSearch,0};
    if (i==clearId) return {ActionKind::ClearSearch,0};
    return i>=0 && i<buttonCount_ ? buttons_[i].action : Action{};
}
bool MuseumPanel::isSearch(int i) const { return i==searchId || i==clearId; }
bool MuseumPanel::switchesMode(int i) const {
    const auto k=action(i).kind;
    return k==ActionKind::SelectSection || k==ActionKind::SelectCategory ||
           k==ActionKind::ShowTransfer || k==ActionKind::ShowCollection;
}
void MuseumPanel::draw(Renderer& r,const MuseumState& state,int over,int pressed,unsigned long ticks) const {
    if (!visible_) return;
    const int font=static_cast<int>(pixel(style_.fontSize*scale_));
    const float pad=pixel(style_.textPadding*scale_);
    r.fill(layout_.panel,ground); r.outline(layout_.panel,hover);
    for (int i=0;i<buttonCount_;++i) buttons_[i].draw(r,font,pad,i==over,i==pressed);
    r.fill(layout_.search,{0.180f,0.165f,0.122f,1});
    r.outline(layout_.search,state.searchFocused ? gold : hover);
    wchar_t text[maxQueryLength+2]; std::wmemcpy(text,state.searchText,maxQueryLength+1);
    int n=static_cast<int>(std::wcslen(text));
    if (state.searchFocused && ((ticks/500u)&1u)==0u) { text[n++]=L'_'; text[n]=0; }
    TextRenderer tr(r);
    const Rect area=layout_.searchText.inset(pad);
    const wchar_t* start=text;
    while (*start && r.measure(start,font)>area.w) ++start;
    if (n>0) r.text(area,start,font,gold,TextAlign::Left);
    else r.text(area,state.searchEnabled ? L"Search" : L"Search off",font,hover,TextAlign::Left);
    if (state.searchText[0]) tr.drawCentered(layout_.clearSearch,L"x",font,gold);
    StatisticsView{layout_.statistics}.draw(r,state,font,
        static_cast<int>(pixel(style_.statsMinFontSize*scale_)),pad);
}
}
