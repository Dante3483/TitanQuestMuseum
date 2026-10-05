#include <cstdio>
#include "backend/localization.h"
#include "ui/museum_panel.h"
#include <cwchar>
namespace museum::ui {
namespace { constexpr int searchId = 1000, clearId = 1001; }
void MuseumPanel::hide() { visible_=false; buttonCount_=0; searchShown_=false; }
void MuseumPanel::arrange(Rect caravan,float scale,const MuseumState& state,Rect viewport) {
    viewport_=viewport;setsList_=state.setsList;cardCount_=0;
    hide(); scale_=scale; layout_=layoutPanel(caravan,scale,style_);
    if (layout_.statistics.w <= 2*style_.textPadding*scale || layout_.panel.w <= 0) return;
    visible_=true;
    searchShown_ = state.searchEnabled;
    const float gap = pixel(style_.controlGap*scale);
    for (int i=0;i<4;++i) {
        const Section section=static_cast<Section>(i);
        buttons_[buttonCount_++]={rowCell(layout_.sections,4,i,gap),sectionCaption(section),
            {ActionKind::SelectSection,i},state.activeSection==section,true,state.sectionSearchMatch[i]};
    }
    if(state.activeSection==Section::Sets) {
        if(state.setsList) {
            cardFirst_=buttonCount_;
            const float margin=pixel(6*scale),cardGap=pixel(6*scale);
            const float width=(viewport.w-2*margin-cardGap)/2;
            const float height=(viewport.h-2*margin-7*cardGap)/8;
            for(int i=0;i<setsPerSheet && i+state.setsOffset<state.sectionCategoryCount;++i) {
                const auto& c=state.sectionCategories[i+state.setsOffset];
                const Rect rect={viewport.x+margin+(i%2)*(width+cardGap),viewport.y+margin+(i/2)*(height+cardGap),width,height};
                buttons_[buttonCount_++]={rect,c.caption,{ActionKind::SelectCategory,c.id},c.total>0 && c.discovered==c.total,true,c.searchMatch};
                cardOwned_[i]=c.discovered;cardTotal_[i]=c.total;++cardCount_;
            }
        } else {
            buttons_[buttonCount_++]={layout_.categories,i18n::text("museum.back"),{ActionKind::BackToSets,0}};
        }
    } else {
    for (int i=0;i<state.sectionCategoryCount;++i) {
        const auto& c=state.sectionCategories[i];
        buttons_[buttonCount_++]={rowCell(layout_.categories,state.sectionCategoryCount,i,gap),
            c.caption,{ActionKind::SelectCategory,c.id},state.selectedCategory==c.id,true,
            c.searchMatch,state.searchText[0]!=0 && c.indexed && !c.searchMatch};
    }
    }
    buttons_[buttonCount_++]={layout_.owned,i18n::text("museum.owned"),{ActionKind::ToggleOwned,0},state.ownedOnly};
    buttons_[buttonCount_++]={layout_.transfer,i18n::text("museum.transfer"),{ActionKind::ShowTransfer,0},state.mode==Mode::Transfer};
    buttons_[buttonCount_++]={layout_.collection,i18n::text("museum.collection"),{ActionKind::ShowCollection,0},state.mode==Mode::Collection};
}
int MuseumPanel::hit(float x,float y) const {
    if (!visible_) return -1;
    if (layout_.clearSearch.contains(x,y)) return clearId;
    if (layout_.search.contains(x,y)) return searchId;
    for (int i=0;i<buttonCount_;++i) if (buttons_[i].hit(x,y)) return i;

    if(setsList_ && viewport_.contains(x,y)) return 1002;
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
    return k==ActionKind::SelectSection || k==ActionKind::SelectCategory || k==ActionKind::ScrollSets || k==ActionKind::BackToSets ||
           k==ActionKind::ShowTransfer || k==ActionKind::ShowCollection;
}
void MuseumPanel::draw(Renderer& r,const MuseumState& state,int over,int pressed,unsigned long ticks) const {
    if (!visible_) return;
    const int font=static_cast<int>(pixel(style_.fontSize*scale_));
    const float pad=pixel(style_.textPadding*scale_);
    r.fill(layout_.panel,ground); r.outline(layout_.panel,hover);

    if(state.setsList) r.fill(viewport_,{0.180f,0.165f,0.122f,1});
    for (int i=0;i<buttonCount_;++i) {
        if(state.setsList && i>=cardFirst_ && i<cardFirst_+cardCount_) {
            const auto& card=buttons_[i];const int k=i-cardFirst_;const Rect rect=card.rect;
            const float cardPad=pixel(8*scale_);
            const Color complete={0.50f,0.86f,0.42f,1};
            const Color edge=card.marked?Color{0.38f,0.79f,1.0f,1}:card.selected?complete:i==over?gold:hover;
            const Color background=card.marked?Color{0.15f,0.30f,0.40f,1}:card.selected?Color{0.20f,0.30f,0.15f,1}:ground;
            r.fill(rect,i==pressed?dark:i==over && !card.marked && !card.selected?normal:background);
            r.outline(rect,edge,pixel((card.marked?2.0f:1.0f)*scale_));
            wchar_t title[256];widen(card.caption,title,256);TextRenderer text(r);
            const Rect name={rect.x+cardPad,rect.y+cardPad,rect.w-2*cardPad,rect.h*0.34f};
            const int fitted=text.fit(title,font,static_cast<int>(pixel(9*scale_)),name.w);
            if(fitted) r.text(name,title,fitted,gold,TextAlign::Left);
            wchar_t progress[32];std::swprintf(progress,32,L"%d / %d",cardOwned_[k],cardTotal_[k]);
            r.text({rect.x+cardPad,rect.y+rect.h*0.48f,rect.w-2*cardPad,rect.h*0.26f},progress,font,card.selected?complete:gold,TextAlign::Right);
            if(card.selected) {
                wchar_t completed[64];widen(i18n::text("museum.set_completed"),completed,64);
                r.text({rect.x+cardPad,rect.y+rect.h*0.48f,rect.w*0.6f-cardPad,rect.h*0.26f},completed,font,complete,TextAlign::Left);
            }
            const Rect bar={rect.x+cardPad,rect.y+rect.h-cardPad-pixel(6*scale_),rect.w-2*cardPad,pixel(6*scale_)};
            r.fill(bar,{0.055f,0.047f,0.027f,1});
            const float border=pixel(scale_);
            const Rect fill=bar.inset(border);
            if(cardTotal_[k]>0 && cardOwned_[k]>0)
                r.fill({fill.x,fill.y,fill.w*cardOwned_[k]/cardTotal_[k],fill.h},
                       card.selected?Color{0.42f,0.80f,0.35f,1}:Color{0.95f,0.73f,0.24f,1});
            r.outline(bar,{0.72f,0.63f,0.39f,1},border);
        } else buttons_[i].draw(r,font,pad,i==over,i==pressed);
    }
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
    else if (!state.searchFocused) { wchar_t label[64]; widen(i18n::text(state.searchEnabled ? "museum.search" : "museum.search_off"),label,64);
        r.text(area,label,font,hover,TextAlign::Left); }
    if (state.searchText[0]) tr.drawCentered(layout_.clearSearch,L"x",font,gold);
    StatisticsView{layout_.statistics}.draw(r,state,font,
        static_cast<int>(pixel(style_.statsMinFontSize*scale_)),pad);
}
}
