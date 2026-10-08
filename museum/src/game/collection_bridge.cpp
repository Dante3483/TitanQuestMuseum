#include <cstring>
#include "backend/localization.h"
#include "game/collection_bridge.h"
#include "game/item_adapter.h"
#include "backend/collection_runtime.h"
#include "game/view_adapter.h"
#include "game/ownership_adapter.h"
#include "game/search_adapter.h"
#include "core/configuration.h"
#include <algorithm>
#include "core/paths.h"

namespace museum::game {
namespace { bool setsList=false; int setsOffset=0; }
bool initializeCollection(void* module) {
    char path[MAX_PATH];
    if (!integration::utModFile(static_cast<HMODULE>(module),"uniq-groups.txt",path,sizeof(path))) return false;
    char directory[MAX_PATH];
    if (GetModuleFileNameA(static_cast<HMODULE>(module),directory,MAX_PATH)) {
        char* slash=strrchr(directory,'\\');
        if (slash) { *slash=0; i18n::load(directory,integration::g_cfg.textLanguage); }
    }
    integration::collectionOwnershipProvider(integration::ownedKnown,integration::ownedRecordState);
    return integration::collectionInitialize(path,integration::g_cfg.ownedOnly!=0);
}
CollectionSnapshot readCollection() {
    using namespace integration;
    CollectionSnapshot s;
    if(categorySection(liveGroupLabel(liveWantedGroup()))!=Section::Sets) setsList=false;
    s.setsList=setsList && viewOn(); s.setsOffset=setsOffset;
    s.mode = viewOn() ? Mode::Collection : Mode::Transfer;
    s.selectedCategory = liveWantedGroup();
    s.shownCategory = liveShownGroup();
    s.ownedOnly = liveOwnedOnly();
    s.searchEnabled = searchFieldWanted();
    s.searchFocused = searchFieldFocused();
    searchFieldText(s.searchText, maxQueryLength + 1);
    s.categoryCount = (std::min)(liveGroupCount(), maxCategories);
    const unsigned marks = searchMarks(), indexed = liveSearchIndexed();
    for (int i = 0; i < s.categoryCount; ++i) {
        auto& c = s.categories[i]; c.id = i;
        const char* label = liveGroupLabel(i);
        c.caption = categoryCaption(label); c.section = categorySection(label);
        c.total = liveGroupEntries(i);
        c.searchMatch = c.section==Section::Sets ? liveGroupSearchMatch(i) : i<32 && (marks & (1u << i)) != 0;
        c.discovered=liveGroupOwnedCount(i);
        c.indexed = i<32 && g_cfg.searchButtons && (indexed & (1u << i)) != 0;
    }
    s.categoryCounts.known = ownedLabel(&s.categoryCounts.owned, &s.categoryCounts.total);
    if (!s.categoryCounts.known) s.categoryCounts.total = liveGroupEntries(s.shownCategory);
    if(s.setsList) {
        s.categoryCounts.owned=0;s.categoryCounts.total=0;
        for(int i=0;i<s.categoryCount;++i) if(s.categories[i].section==Section::Sets) {
            if(s.categories[i].total>0 && s.categories[i].discovered==s.categories[i].total) ++s.categoryCounts.owned;
            ++s.categoryCounts.total;
        }
    }
    s.allCounts.known = ownedLabelAll(&s.allCounts.owned, &s.allCounts.total);
    if (!s.allCounts.known) {
        s.allCounts.total = 0;
        for (int i = 0; i < s.categoryCount; ++i) if(s.categories[i].section!=Section::Sets) s.allCounts.total += s.categories[i].total;
    }
    if (s.mode == Mode::Collection) searchLabel(s.searchStatus, sizeof(s.searchStatus));
    else {
        char brief[24];
        searchRealLabel(s.searchStatus, sizeof(s.searchStatus), brief, sizeof(brief));
    }
    s.visibleCount = s.mode == Mode::Collection ? (std::min)(protoCount(), maxVisibleItems) : 0;
    for (int i = 0; i < s.visibleCount; ++i) {
        const char* record = nullptr;
        if (!protoAt(i, &record, nullptr, nullptr, nullptr)) continue;
        s.visibleItems[i] = {record, ownedRecordState(record) > 0, searchHighlight(i, record)};
    }
    return s;
}
void apply(Action a) {
    using namespace integration;
    switch (a.kind) {
    case ActionKind::ShowTransfer: viewRequest(kUtViewReqOff); break;
    case ActionKind::ShowCollection: viewRequest(kUtViewReqOn); break;
    case ActionKind::ToggleOwned: liveToggleOwnedOnly(); break;
    case ActionKind::SelectCategory:
        setsList=false;
        if (liveSelectGroup(a.value)) viewRequest(kUtViewReqOn);
        break;
    case ActionKind::BackToSets: setsList=true; break;
    case ActionKind::ScrollSets: {
        int count=0;for(int i=0;i<liveGroupCount();++i) if(categorySection(liveGroupLabel(i))==Section::Sets) ++count;
        setsOffset=(std::max)(0,(std::min)((std::max)(0,((count+1)/2-setsPerSheet/2)*2),setsOffset+a.value*2));
        break;
    }
    case ActionKind::SelectSection:
        setsList=static_cast<Section>(a.value)==Section::Sets;
        for (int i = 0; i < liveGroupCount(); ++i)
            if (categorySection(liveGroupLabel(i)) == static_cast<Section>(a.value)) {
                liveSelectGroup(i); viewRequest(kUtViewReqOn); break;
            }
        break;
    case ActionKind::FocusSearch:
        if (searchFieldWanted()) searchFieldFocus(); else searchFieldOffClick();
        break;
    case ActionKind::ClearSearch:
        if (searchFieldWanted()) {
            wchar_t text[maxQueryLength + 1];
            if (searchFieldText(text, maxQueryLength + 1) > 0) searchFieldClearQuery();
            else searchFieldFocus();
        }
        break;
    default: break;
    }
}
void blurSearch() { integration::searchFieldBlur("a click outside the field"); }
}

namespace museum::game { bool setsBrowseActive() { return setsList && integration::viewOn(); } }
