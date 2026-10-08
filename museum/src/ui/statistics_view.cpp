#include "backend/localization.h"
#include "ui/statistics_view.h"
#include <cstdio>
namespace museum::ui {
void StatisticsView::draw(Renderer& r,const MuseumState& state,int base,int minimum,float padding) const {
    char line[192], category[32], all[32];
    const Counts c = state.categoryCounts, a = state.allCounts;
    if (c.known) std::snprintf(category,sizeof(category),"%d/%d",c.owned,c.total);
    else std::snprintf(category,sizeof(category),"?/%d",c.total);
    if (a.known) std::snprintf(all,sizeof(all),"%d/%d",a.owned,a.total);
    else std::snprintf(all,sizeof(all),"?/%d",a.total);
    const char* query = state.searchStatus;
    if (state.mode == Mode::Collection)
        std::snprintf(line,sizeof(line),"%s %s%s%s | %s %s",state.shownCategoryName,category,
                      query[0] ? " | " : "",query,i18n::text("museum.all"),all);
    else std::snprintf(line,sizeof(line),"%s%s%s | %s %s",i18n::text("museum.transfer"),query[0] ? " | " : "",query,i18n::text("museum.all"),all);
    wchar_t text[192]; widen(line,text,192);
    TextRenderer tr(r);
    const Rect area = {rect.x+padding,rect.y+padding,rect.w-padding,rect.h-2*padding};
    const int size = tr.fit(text,base,minimum,area.w);
    if (size) tr.drawRight(area,text,size,{0.847f,0.776f,0.565f,1});
}
}
