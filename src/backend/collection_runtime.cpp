#include "backend/model/catalogue.h"
// ut_live.cpp - the collection page model (see ut_live.h). TQ port of GD's ut_live.cpp: the group
// table, the want/shown pair written by the input and read by the tick, and the wheel / key /
// select entry points keep GD's names and semantics; the reagent-box capture and the display
// prototype cache are not ported (TQ's page is a sack of prototypes). GD's owned-only
// filter IS ported (the OWN button): the page is laid out by gdut::placePage over the owned flags.
//
// Invariants: the input path writes only g_wantGroup / g_wantPage / g_dirty (interlocked); the
// group table is built once on the worker before any hook and never changes afterwards, so the
// game thread reads it without a lock.

#include "backend/collection_runtime.h"

#include <stdio.h>
#include <cstring>

#include <string>
#include <vector>

#include "backend/model/layout.h"

#include "backend/navigation_events.h"   // UtNavBurst
#include "core/configuration.h"
#include "core/logging.h"


#include "backend/search_rules.h"
#include "backend/window_math.h"

namespace integration {
namespace {
gdut::Catalogue catalogue;
bool (*ownershipKnown)() = nullptr;
int (*recordOwnership)(const char*) = nullptr;

struct Group {
    std::string label;
    std::vector<std::string> records;   // uniq-groups.txt order
    std::vector<unsigned char> fw, fh;  // footprints
    std::vector<gdut::PackPlacement> placed;
    gdut::SlotGeometry slot;            // one slot size per group (its largest footprint)
    int pages = 0;
    std::vector<unsigned char> owned;   // the OWN filter's flags (sized once, at init)
    int ownedCount = 0;
    std::vector<unsigned char> match;   // the search's flags (sized once, at init)
    bool nameMatch = false;
    int hitCount = 0;                   // the matches that pass OWN, while a query stands
};

std::vector<Group>* g_groups = nullptr;
int g_pagesTotal = 0;
volatile LONG g_wantGroup = 0;
// the WINDOW's first slot row. The name stays GD's "page" in the API, the
// unit is a slot row: the wheel moves it by one, < > / PgUp / PgDn by a window (the group's rows).
volatile LONG g_wantPage = 0;
volatile LONG g_dirty = 0;
int g_shownGroup = 0;   // game thread
int g_shownPage = 0;    // the shown window's first slot row
int g_shownRows = 0;    // slot rows of the shown group (the records shown, OWN included)
int g_windowRows = 0;   // slot rows per window of the shown group
// NO settle (the earlier kWheelSettleMs / kWheelHoldMs and the held rebuild are gone). A
// wheel tick marks the window dirty like any navigation and the NEXT GameEngine::Update rebuilds it;
// ticks between two Updates coalesce into one step (ut_viewgate.h UtNavBurst). Game thread.
UtNavBurst g_nav = {0, 0};     // inputs since the last taken rebuild
UtNavBurst g_taken = {0, 0};   // what liveTakeDirty / liveDropDirty took for the rebuild now
char g_status[96] = "groups=0";
// - the OWN filter. Game thread (the pad's click, the view tick, ut_owned's refresh).
bool g_ownedOnly = false;      // asked for (the ini's owned_only, then the OWN button)
bool g_ownKnown = false;       // the flags below come from a KNOWN owned set
bool g_ownPersist = false;     // the tick writes owned_only back
int g_shownCount = 0;          // records shown over all pages of the shown group (last build)
// - the property search (ut_search): a query stands, the groups fully indexed, and what the last
// apply found. The marks are read by the pad's draw.
bool g_searchOn = false;
unsigned g_searchIndexed = 0;
volatile LONG g_searchMarks = 0;
volatile LONG g_searchIndexedPub = 0;   // g_searchIndexed while a query stands, for the pad
volatile LONG g_searchFound = 0;
volatile LONG g_searchGroups = 0;
// the last built page: the record and its index in the shown group, per place (the highlight)
const char* g_placeRec[kUtProtoMax];
int g_placeK[kUtProtoMax];
int g_placeGroup[kUtProtoMax];
int g_placeN = 0;

bool readAll(const char* path, std::string* out) {
    FILE* fh = nullptr;
    if (fopen_s(&fh, path, "rb") != 0 || !fh) return false;
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fh)) > 0) out->append(buf, n);
    fclose(fh);
    return !out->empty();
}

void markDirty() { InterlockedExchange(&g_dirty, 1); }

// The search's counts and marks, from the match, owned and OWN state as it is now.
void searchRecompute() {
    if (!g_groups) return;
    // Item text respects Discovery; source matches can mark anonymous silhouettes.

    unsigned marks = 0;
    int found = 0, groups = 0;
    for (size_t gi = 0; gi < g_groups->size(); ++gi) {
        Group& g = (*g_groups)[gi];
        const int n = (int)g.records.size();
        g.hitCount=0;
        if(g_searchOn) for(int k=0;k<n;++k)
            if((g.match[k]&2) || (g_ownKnown && g.owned[k] && (g.match[k]&1))) ++g.hitCount;
        if (!g_searchOn) continue;
        if (g.label.compare(0,4,"Set:")!=0) {
            found += g.hitCount;
            if (g.hitCount > 0) ++groups;
        }
        if (gi < 32 && g.hitCount>0)
            marks |= 1u << (unsigned)gi;
    }
    InterlockedExchange(&g_searchMarks, (LONG)marks);
    InterlockedExchange(&g_searchIndexedPub, g_searchOn ? (LONG)g_searchIndexed : 0);
    InterlockedExchange(&g_searchFound, found);
    InterlockedExchange(&g_searchGroups, groups);
}

// an INPUT navigation (not a deposit / owned refresh): the burst the rebuild reports.
void noteNav(bool wheel) {
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    utNavNote(g_nav, wheel, q.QuadPart);
}

}  // namespace

void collectionOwnershipProvider(bool (*known)(), int (*state)(const char*)) {
    ownershipKnown=known; recordOwnership=state;
}
bool collectionInitialize(const char* path, bool ownedOnly) {
    if (g_groups) return true;
    if (!path || !*path) return false;
    try {
        std::string text;
        if (!readAll(path, &text)) {
            logW("live: uniq-groups.txt could not be read - the view is unavailable");
            return false;
        }
        std::vector<gdut::GroupList> lists;
        std::string err;
        if (!gdut::parseGroupsText(text, &lists, &err)) {
            logW("live: %s - the view is unavailable", err.c_str());
            return false;
        }
        std::vector<Group>* gs = new std::vector<Group>();
        size_t entries = 0;
        for (const gdut::GroupList& l : lists) {
            Group g;
            g.label = l.label;
            std::vector<gdut::PackItem> items;
            for (size_t k = 0; k < l.entries.size(); ++k) {
                g.records.push_back(l.entries[k].record);
                g.fw.push_back((unsigned char)l.entries[k].w);
                g.fh.push_back((unsigned char)l.entries[k].h);
                items.push_back(gdut::PackItem{(int)k, l.entries[k].w, l.entries[k].h});
            }
            g.pages = gdut::packSlots(items, gdut::kHostCols, gdut::kHostRows, &g.placed, &g.slot);
            g.owned.assign(l.entries.size(), 0);
            g.match.assign(l.entries.size(), 0);
            if (g.pages < 1) {
                logW("live: group %s cannot be packed into 16 x 15 pages - the view is unavailable",
                     l.label.c_str());
                delete gs;
                return false;
            }
            logD("live: group %s - %zu records, slot %dx%d, %d x %d = %d per page, %d page(s)",
                 l.label.c_str(), l.entries.size(), g.slot.w, g.slot.h, g.slot.cols, g.slot.rows,
                 g.slot.perPage(), g.pages);
            entries += l.entries.size();
            g_pagesTotal += g.pages;
            gs->push_back(g);
        }
        // Immutable alternate views of existing items: never add these to completion totals.
        const std::string groupPath(path);
        const auto slash=groupPath.find_last_of("/\\");
        const std::string cataloguePath=groupPath.substr(0,slash+1)+"catalogue.bin";
        std::string why;
        if (catalogue.loadFromFile(cataloguePath,&why)) {
            for (const auto& set : catalogue.sets()) {
                if (!set.memberCount || gs->size()>=160) continue;
                Group g; g.label="Set:"+std::string(set.name);
                std::vector<gdut::PackItem> items;
                for (unsigned k=0;k<set.memberCount;++k) {
                    const auto& item=catalogue.item(set.members[k]);
                    const std::string record(item.record);
                    bool visible=false;
                    for (const auto& base:*gs) {
                        if (base.label.compare(0,4,"Set:")==0) break;
                        for(const auto& r:base.records) if(r==record) {visible=true;break;}
                        if(visible) break;
                    }
                    if(!visible) continue;
                    g.records.push_back(record);g.fw.push_back(item.footW);g.fh.push_back(item.footH);
                    items.push_back({(int)items.size(),item.footW,item.footH});
                }
                if(items.empty()) continue;
                // A set mixes footprints: reserve half a native cell on each side of
                // its largest member. Keep record footprints unchanged for prototypes.
                auto paddedSlots=items;
                for(auto& slot:paddedSlots) { ++slot.w; ++slot.h; }
                g.pages=gdut::packSlots(paddedSlots,16,15,&g.placed,&g.slot);
                g.owned.assign(items.size(),0);g.match.assign(items.size(),0);
                gs->push_back(g);
            }
        } else logW("sets: catalogue unavailable: %s",why.c_str());
        g_groups = gs;
        g_ownedOnly = ownedOnly;   // the start state (configReload ran before)
        logI("live: %zu groups, %zu records on %d pages of 16 x 15, one slot size per group "
             "(uniq-groups.txt)", gs->size(), entries, g_pagesTotal);
        return true;
    } catch (...) {
        logW("live: out of memory building the group table - the view is unavailable");
        return false;
    }
}

bool liveActive() { return g_groups != nullptr && !g_groups->empty(); }

const gdut::ItemView* liveItemInfo(const char* record) {
    const int index=record ? catalogue.indexOfRecord(record) : -1;
    return index<0 ? nullptr : &catalogue.item(size_t(index));
}

int liveGroupCount() { return g_groups ? (int)g_groups->size() : 0; }

const char* liveGroupLabel(int group) {
    if (!g_groups || group < 0 || group >= (int)g_groups->size()) return "?";
    return (*g_groups)[(size_t)group].label.c_str();
}

int liveGroupEntries(int group) {
    if (!g_groups || group < 0 || group >= (int)g_groups->size()) return -1;
    return (int)(*g_groups)[(size_t)group].records.size();
}

const char* liveGroupRecord(int group, int k) {
    if (!g_groups || group < 0 || group >= (int)g_groups->size()) return nullptr;
    const Group& g = (*g_groups)[(size_t)group];
    return (k >= 0 && k < (int)g.records.size()) ? g.records[(size_t)k].c_str() : nullptr;
}

int livePageCount(int group) {
    if (!g_groups || group < 0 || group >= (int)g_groups->size()) return 0;
    const Group& g = (*g_groups)[(size_t)group];
    return liveOwnedOnlyActive() ? gdut::pagesFor(g.slot, g.ownedCount) : g.pages;
}

namespace {
// the records the wanted group shows now (OWN: its owned count) and the offset clamp.
int shownOf(const Group& g) {
    return liveOwnedOnlyActive() ? g.ownedCount : (int)g.records.size();
}
bool stepRows(int delta, const char* what, bool wheel = false) {
    if (!liveActive()) return false;
    const int gi = liveWantedGroup();
    const Group& g = (*g_groups)[(size_t)gi];
    const int from = (int)InterlockedCompareExchange(&g_wantPage, 0, 0);
    const int to = gdut::clampRowOffset(g.slot, shownOf(g), from + delta);
    if (to == from) return false;
    InterlockedExchange(&g_wantPage, to);
    noteNav(wheel);   // before the dirty flag, so the rebuild that takes it reports it
    markDirty();
    logD("live: %s - %s rows %d-%d of %d (from row %d)", what, g.label.c_str(), to + 1,
         to + g.slot.rows, gdut::slotRowsFor(g.slot, shownOf(g)), from + 1);
    return true;
}
}  // namespace

bool liveOwnedOnly() { return g_ownedOnly; }
bool liveOwnedOnlyActive() { return g_ownedOnly && g_ownKnown; }

bool liveToggleOwnedOnly() {
    if (!liveActive()) return false;
    g_ownedOnly = !g_ownedOnly;
    g_ownPersist = true;
    if (g_searchOn) searchRecompute();   // the counts and the marks follow OWN
    InterlockedExchange(&g_wantPage, 0);   // GD: a filter change starts the group at its top
    noteNav(false);
    markDirty();
    logI("live: OWN %s - %s", g_ownedOnly ? "ON" : "OFF",
         !g_ownedOnly ? "every record of the group"
         : g_ownKnown ? "only the records you own"
                      : "asked for; the owned set is not known yet, every record is shown");
    return true;
}

void livePersistOwnedOnly() {
    if (!g_ownPersist) return;
    g_ownPersist = false;
    if (!configPersistInt("owned_only", g_ownedOnly ? 1 : 0))
        logI("live: owned_only=%d could not be written back to the ini (kept for this session)",
             g_ownedOnly ? 1 : 0);
}

void liveOwnedChanged() {
    if (!liveActive()) return;
    const bool known = ownershipKnown && recordOwnership && ownershipKnown();
    bool changed = known != g_ownKnown;
    for (Group& g : *g_groups) {
        int count = 0;
        for (size_t k = 0; k < g.records.size(); ++k) {
            const unsigned char f = known && recordOwnership(g.records[k].c_str()) == 1 ? 1 : 0;
            if (f != g.owned[k]) changed = true;
            g.owned[k] = f;
            count += f;
        }
        g.ownedCount = count;
    }
    g_ownKnown = known;
    if (changed && g_searchOn) searchRecompute();   // the marks follow the owned set
    if (changed && g_ownedOnly) markDirty();   // the filtered page follows the set
}

bool liveEmptyPageOk() { return liveOwnedOnlyActive() && g_shownCount == 0; }

void liveSearchSetMatch(int group, int k, bool match, bool sourceMatch) {
    if (!g_groups || group < 0 || group >= (int)g_groups->size()) return;
    Group& g = (*g_groups)[(size_t)group];
    if (k >= 0 && k < (int)g.match.size()) g.match[(size_t)k] = (match ? 1 : 0) | (sourceMatch ? 2 : 0);
}

void liveSearchApply(bool active, unsigned indexedMask) {
    if (!liveActive()) return;
    g_searchOn = active;
    g_searchIndexed = indexedMask;
    searchRecompute();   // the page stays as it is: the highlight is drawn per frame
}

bool liveSearchHighlight(int i, const char* record) {
    if (!g_searchOn || !g_groups || !record || g_shownGroup < 0 ||
        g_shownGroup >= (int)g_groups->size())
        return false;
    int place=i;
    if(place<0 || place>=g_placeN || !g_placeRec[place] || std::strcmp(g_placeRec[place],record)!=0) {
        place=-1;
        for(int j=0;j<g_placeN;++j) if(g_placeRec[j] && std::strcmp(g_placeRec[j],record)==0) {place=j;break;}
    }
    if(place<0) return false;
    const int gi=g_placeGroup[place];
    const Group& g = (*g_groups)[(size_t)gi];
    const int k = utSearchPlaceIndex(g_placeRec, g_placeK, g_placeN, place, record);
    return k>=0 && k<(int)g.match.size() && ((g.match[k]&2) ||
           (g_ownKnown && g.owned[k] && (g.match[k]&1)));
}

unsigned liveSearchMarks() { return (unsigned)InterlockedCompareExchange(&g_searchMarks, 0, 0); }

unsigned liveSearchIndexed() {
    return (unsigned)InterlockedCompareExchange(&g_searchIndexedPub, 0, 0);
}

int liveSearchFound(int* groups) {
    if (groups) *groups = (int)InterlockedCompareExchange(&g_searchGroups, 0, 0);
    return (int)InterlockedCompareExchange(&g_searchFound, 0, 0);
}

int liveSearchShownIn(int group, int* of) {
    if (of) *of = liveGroupEntries(group);
    if (!g_groups || group < 0 || group >= (int)g_groups->size()) return 0;
    const Group& g = (*g_groups)[(size_t)group];
    return g_searchOn ? g.hitCount : (int)g.records.size();
}

int livePagesTotal() { return g_pagesTotal; }
int liveShownGroup() { return g_shownGroup; }
int liveShownPage() { return g_shownPage; }
int liveWantedGroup() { return (int)InterlockedCompareExchange(&g_wantGroup, 0, 0); }

bool liveSelectGroup(int group) {
    if (!liveActive() || group < 0 || group >= liveGroupCount()) return false;
    InterlockedExchange(&g_wantGroup, group);
    InterlockedExchange(&g_wantPage, 0);
    noteNav(false);
    markDirty();
    return true;
}

// a WINDOW step (< > / PgUp / PgDn): the offset moves by the group's slot rows per window,
// clamped - so the last step lands on the last FULL window (it overlaps the one before it).
bool liveStepPage(int dir) {
    if (!liveActive()) return false;
    const Group& g = (*g_groups)[(size_t)liveWantedGroup()];
    return stepRows((dir < 0 ? -1 : 1) * (g.slot.rows > 0 ? g.slot.rows : 1), "window step");
}

// one slot row (the wheel over the page). rebuilt on the next Update, no settle.
bool liveStepRow(int dir) { return stepRows(dir < 0 ? -1 : 1, "wheel", true); }

bool liveHandleWheel(int ticks, bool ctrl) {
    if (!liveActive() || ticks == 0) return false;
    // ut_panel's cycleStep owns the Ctrl cycle (Transfer is a stop) and calls this
    // with ctrl=false only; the Ctrl branch is kept for GD's signature and answers false at the ends.
    if (ctrl) {   // the groups inside the cycle; the ends go to Transfer (ut_panel's step)
        const int to = utGroupCycleStep(liveWantedGroup(), liveGroupCount(), ticks > 0 ? -1 : 1);
        return to >= 0 && liveSelectGroup(to);
    }
    liveStepRow(ticks > 0 ? -1 : 1);   // one slot row; consumed even at the end, as GD's wheel
    return true;
}

bool liveHandleKey(int vk, bool ctrl) {
    if (vk != VK_PRIOR && vk != VK_NEXT) return false;
    if (ctrl) return liveHandleWheel(vk == VK_PRIOR ? 1 : -1, true);
    liveStepPage(vk == VK_PRIOR ? -1 : 1);   // PgUp / PgDn move a whole window
    return true;
}

bool liveTakeDirty() {
    if (InterlockedExchange(&g_dirty, 0) == 0) return false;
    g_taken = utNavTake(g_nav);   // every input since the last Update, in this one step
    return true;
}

void liveDropDirty() {
    g_taken = utNavTake(g_nav);   // the view's own build (ON) is the input's window: it reports it
    InterlockedExchange(&g_dirty, 0);
}

bool liveRebuildInput(int* wheelTicks, long long* firstQpc) {
    const UtNavBurst t = utNavTake(g_taken);   // reported once
    if (wheelTicks) *wheelTicks = t.ticks;
    if (firstQpc) *firstQpc = t.firstQpc;
    return t.firstQpc != 0;
}

int liveShownRows() { return g_shownRows; }
int liveWindowRows() { return g_windowRows; }

int liveLeftover(UtCellRect* out, int cap) {
    if (!liveActive() || !out || cap < 3) return 0;
    const Group& g = (*g_groups)[(size_t)g_shownGroup];
    if(g.label.compare(0,4,"Set:")==0) return 0;
    gdut::CellRect r[3];
    const int n = gdut::windowLeftover(g.slot, g_shownCount, g_shownPage, gdut::kHostCols,
                                       gdut::kHostRows, r);
    int count = 0;
    for (int i = 0; i < n; ++i) {
        const int top = gdut::filledRowEdge(g.slot, r[i].row, gdut::kHostRows);
        const int bottom = gdut::filledRowEdge(g.slot, r[i].row+r[i].h, gdut::kHostRows);
        if (bottom > top) out[count++] = UtCellRect{r[i].col, top, r[i].w, bottom-top};
    }
    return count;
}

void liveMarkDirty() { markDirty(); }

bool liveHasRecord(const char* folded) {
    if (!g_groups || !folded || !*folded) return false;
    for (size_t g = 0; g < g_groups->size(); ++g) {
        const std::vector<std::string>& r = (*g_groups)[g].records;
        for (size_t k = 0; k < r.size(); ++k) {
            // the catalogue's paths are already lower case with '/' (uniq-groups.txt)
            if (_stricmp(r[k].c_str(), folded) == 0) return true;
        }
    }
    return false;
}

int livePagePlaces(UtProtoPlace* out, int cap) {
    if (!liveActive()) return 0;
    const int g = liveWantedGroup();
    int page = (int)InterlockedCompareExchange(&g_wantPage, 0, 0);
    const Group& gr = (*g_groups)[(size_t)g];
    // one pure placement for both views - every record, or the OWN filter's owned ones in
    // the same slots (gdut::placePage; packSlots is its unfiltered special case, model_test).
    // `page` is the window's first SLOT ROW, clamped so the window stays full (the model's
    // placeWindow / clampRowOffset, model_test "window").
    const bool own = liveOwnedOnlyActive();
    page = gdut::clampRowOffset(gr.slot, own ? gr.ownedCount : (int)gr.records.size(), page);
    if (page != (int)InterlockedCompareExchange(&g_wantPage, 0, 0))
        InterlockedExchange(&g_wantPage, page);   // the filter shrank the group: clamp
    static gdut::PackPlacement placed[kUtProtoMax];
    const int want = cap < kUtProtoMax ? cap : kUtProtoMax;
    const int m = gdut::placeWindow(gr.slot, gr.fw.data(), gr.fh.data(), (int)gr.records.size(),
                                    own ? gr.owned.data() : nullptr,
                                    page, placed, want,
                                    &g_shownCount);
    int n = 0;
    for (int i = 0; i < m; ++i) {
        gdut::PackPlacement p = placed[i];
        int slotWidth = gr.slot.w;
        int slotHeight = gr.slot.h;
        if(gr.label.compare(0,4,"Set:")==0) {
            // Spread unused native columns across the set's slots; keep icon footprints intact.
            const int column=p.slotCol/gr.slot.w;
            const int left=column*gdut::kHostCols/gr.slot.cols;
            const int right=(column+1)*gdut::kHostCols/gr.slot.cols;
            p.slotCol=left;slotWidth=right-left;
            p.col=left+(slotWidth-gr.fw[(size_t)p.index])/2;
        }
        // Set cells use the maximum member footprint without stretching rows over the canvas.
        if(gr.label.compare(0,4,"Set:")!=0)
            gdut::fillWindowSlot(gr.slot, gdut::kHostRows, gr.fh[(size_t)p.index], &p, &slotHeight);
        out[n].record = gr.records[(size_t)p.index].c_str();
        out[n].col = p.col;
        out[n].row = p.row;
        out[n].w = gr.fw[(size_t)p.index];
        out[n].h = gr.fh[(size_t)p.index];
        out[n].slotCol = p.slotCol;
        out[n].slotRow = p.slotRow;
        out[n].slotW = slotWidth;
        out[n].slotH = slotHeight;
        g_placeRec[n] = out[n].record;   // the highlight's record -> index map
        g_placeGroup[n]=g;
        g_placeK[n] = p.index;
        ++n;
    }
    g_placeN = n;
    g_shownGroup = g;
    g_shownPage = page;
    g_shownRows = gdut::slotRowsFor(gr.slot, g_shownCount);
    g_windowRows = gr.slot.rows;
    _snprintf_s(g_status, sizeof(g_status), _TRUNCATE, "groups=%d pages=%d shown=%d/%d",
                liveGroupCount(), g_pagesTotal, g, page);
    return n;
}

const char* liveStatus() { return g_status; }

// Convert integer sack row boundaries to equal fractional visual row heights.
float liveVisualCell(float cell, bool inverse) {
    if (!liveActive() || g_shownGroup < 0 || g_shownGroup >= (int)g_groups->size()) return cell;
    if((*g_groups)[(size_t)g_shownGroup].label.compare(0,4,"Set:")==0) return cell;
    const auto& g=(*g_groups)[(size_t)g_shownGroup].slot;
    if (g.rows<=0 || g.h<=0 || cell<0 || cell>15) return cell;
    for (int i=0;i<g.rows;++i) {
        const float a=(float)gdut::filledRowEdge(g,i*g.h,15);
        const float b=(float)gdut::filledRowEdge(g,(i+1)*g.h,15);
        const float va=15.0f*i/g.rows, vb=15.0f*(i+1)/g.rows;
        const float lo=inverse?va:a, hi=inverse?vb:b;
        if (cell<=hi) return (inverse?a:va)+(cell-lo)/(hi-lo)*(inverse?b-a:vb-va);
    }
    return cell;
}

// Map native integer column bounds to equally spaced visual set columns and back.
float liveVisualColumn(float cell,bool inverse) {
    if(!liveActive() || g_shownGroup<0 || g_shownGroup>=liveGroupCount() || cell<0 || cell>16) return cell;
    const auto& group=(*g_groups)[(size_t)g_shownGroup];
    if(group.label.compare(0,4,"Set:")!=0 || group.slot.cols<=0) return cell;
    const int columns=group.slot.cols;
    for(int i=0;i<columns;++i) {
        const float a=(float)(i*16/columns),b=(float)((i+1)*16/columns);
        const float va=16.0f*i/columns,vb=16.0f*(i+1)/columns;
        const float lo=inverse?va:a,hi=inverse?vb:b;
        if(cell<=hi) return (inverse?a:va)+(cell-lo)/(hi-lo)*(inverse?b-a:vb-va);
    }
    return cell;
}

}  // namespace integration

namespace integration { int liveGroupOwnedCount(int g) { return g_ownKnown && g_groups && g>=0 && g<(int)g_groups->size()?(*g_groups)[(size_t)g].ownedCount:0; } }

namespace integration { bool liveGroupSearchMatch(int g) { return g_searchOn && g_groups && g>=0 && g<liveGroupCount() && ((*g_groups)[(size_t)g].nameMatch || (*g_groups)[(size_t)g].hitCount>0); } }

namespace integration { void liveSearchSetNameMatch(int g,bool match) { if(g_groups && g>=0 && g<liveGroupCount()) (*g_groups)[(size_t)g].nameMatch=match; } }
