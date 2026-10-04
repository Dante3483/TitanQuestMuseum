#include "layout.h"
#include "backend/window_math.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
namespace gdut {
// ---- the slot packer ------------------------------------------------------------------

bool slotOf(const std::vector<PackItem>& items, int cols, int rows, SlotGeometry* out) {
    SlotGeometry g;
    if (out) *out = g;
    if (items.empty() || cols <= 0 || rows <= 0) return false;
    for (const PackItem& it : items) {
        if (it.w < 1 || it.h < 1 || it.w > cols || it.h > rows) return false;
        g.w = std::max(g.w, it.w);
        g.h = std::max(g.h, it.h);
    }
    g.cols = cols / g.w;
    g.rows = rows / g.h;
    if (out) *out = g;
    return g.cols >= 1 && g.rows >= 1;
}

int packSlots(const std::vector<PackItem>& items, int cols, int rows,
              std::vector<PackPlacement>* out, SlotGeometry* slot) {
    if (out) out->clear();
    if (slot) *slot = SlotGeometry();
    if (cols <= 0 || rows <= 0) return -1;
    if (items.empty()) return 0;
    SlotGeometry g;
    if (!slotOf(items, cols, rows, &g)) return -1;
    if (slot) *slot = g;
    const int per = g.perPage();
    if (out) out->reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        const PackItem& it = items[i];
        PackPlacement p;
        if (!placeInSlot(g, static_cast<int>(i), it.index, it.w, it.h, &p)) {
            if (out) out->clear();
            return -1;
        }
        if (out) out->push_back(p);
    }
    return static_cast<int>((items.size() + static_cast<std::size_t>(per) - 1) /
                            static_cast<std::size_t>(per));
}

// ---- one place by ordinal, one page (the OWN filter) ---------------------------------

bool placeInSlot(const SlotGeometry& g, int ordinal, int index, int fw, int fh,
                 PackPlacement* out) noexcept {
    const int per = g.perPage();
    if (per <= 0 || g.w < 1 || g.h < 1 || ordinal < 0 || fw < 1 || fh < 1 || fw > g.w ||
        fh > g.h || !out)
        return false;
    const int k = ordinal % per;
    PackPlacement p;
    p.index = index;
    p.page = ordinal / per;
    p.slotCol = (k % g.cols) * g.w;
    p.slotRow = (k / g.cols) * g.h;
    p.col = p.slotCol + (g.w - fw) / 2;
    p.row = p.slotRow + (g.h - fh) / 2;
    *out = p;
    return true;
}

int placePage(const SlotGeometry& g, const unsigned char* fw, const unsigned char* fh, int n,
              const unsigned char* show, int page, PackPlacement* out, int cap,
              int* shown) noexcept {
    int ordinal = 0, placed = 0;
    if (g.perPage() > 0 && fw && fh && n > 0) {
        for (int i = 0; i < n; ++i) {
            if (show && !show[i]) continue;
            PackPlacement p;
            const bool ok = placeInSlot(g, ordinal, i, fw[i], fh[i], &p);
            ++ordinal;
            if (ok && p.page == page && out && placed < cap) out[placed++] = p;
        }
    }
    if (shown) *shown = ordinal;
    return placed;
}

int pagesFor(const SlotGeometry& g, int shown) noexcept {
    const int per = g.perPage();
    if (per <= 0) return 0;
    if (shown <= 0) return 1;
    return (shown + per - 1) / per;
}

// ---- the row-offset window -------------------------------------------------------------

int slotRowsFor(const SlotGeometry& g, int shown) noexcept {
    if (g.cols <= 0 || g.rows <= 0 || shown <= 0) return 0;
    return (shown + g.cols - 1) / g.cols;
}

// GD's utMaxRowOf / utClampRow (the Grim Dawn mod's src\ut_rowmath.h, copied verbatim): a slot
// row of the window is GD's box row, `shown` its total, `cols` / `rows` the window's slots.
int maxRowOffset(const SlotGeometry& g, int shown) noexcept {
    return integration::utMaxRowOf(shown, g.cols, g.rows);
}

int clampRowOffset(const SlotGeometry& g, int shown, int offset) noexcept {
    return integration::utClampRow(offset, maxRowOffset(g, shown));
}

int placeWindow(const SlotGeometry& g, const unsigned char* fw, const unsigned char* fh, int n,
                const unsigned char* show, int rowOffset, PackPlacement* out, int cap,
                int* shown) noexcept {
    int ordinal = 0, placed = 0;
    const int per = g.perPage();
    if (per > 0 && fw && fh && n > 0 && rowOffset >= 0) {
        const int first = rowOffset * g.cols;   // the window's first ordinal
        for (int i = 0; i < n; ++i) {
            if (show && !show[i]) continue;
            const int k = ordinal - first;
            ++ordinal;
            if (k < 0 || k >= per) continue;
            PackPlacement p;
            if (placeInSlot(g, k, i, fw[i], fh[i], &p) && out && placed < cap) out[placed++] = p;
        }
    }
    if (shown) *shown = ordinal;
    return placed;
}

int windowLeftover(const SlotGeometry& g, int shown, int rowOffset, int hostCols, int hostRows,
                   CellRect out[3]) noexcept {
    if (!out || hostCols <= 0 || hostRows <= 0) return 0;
    int m = 0;
    const int per = g.perPage();
    int inWin = per > 0 ? shown - rowOffset * g.cols : 0;
    if (inWin < 0) inWin = 0;
    if (inWin > per) inWin = per;
    if (per <= 0 || inWin == 0) {   // nothing shown: every cell is left over
        out[m++] = CellRect{0, 0, hostCols, hostRows};
        return m;
    }
    const int fullRows = inWin / g.cols, rem = inWin % g.cols;
    const int usedW = g.cols * g.w;
    if (usedW < hostCols && fullRows > 0)
        out[m++] = CellRect{usedW, 0, hostCols - usedW, fullRows * g.h};
    if (rem > 0) out[m++] = CellRect{rem * g.w, fullRows * g.h, hostCols - rem * g.w, g.h};
    const int top = (fullRows + (rem > 0 ? 1 : 0)) * g.h;
    if (top < hostRows) out[m++] = CellRect{0, top, hostCols, hostRows - top};
    return m;
}

namespace {

void splitTabs(const std::string& line, std::vector<std::string>* f) {
    f->clear();
    std::size_t a = 0;
    for (;;) {
        const std::size_t t = line.find('\t', a);
        f->push_back(line.substr(a, t == std::string::npos ? std::string::npos : t - a));
        if (t == std::string::npos) break;
        a = t + 1;
    }
}

bool toInt(const std::string& s, int* v) {
    if (s.empty() || s.size() > 9) return false;
    int n = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        n = n * 10 + (c - '0');
    }
    *v = n;
    return true;
}

// the G line's cols / rows / cellW / cellH must be the slot of its entries (a group with no
// entry has no slot; its geometry is not checked).
bool slotMatches(const GroupList& g) {
    if (g.entries.empty()) return true;
    std::vector<PackItem> items;
    items.reserve(g.entries.size());
    for (const GroupEntry& e : g.entries) items.push_back(PackItem{0, e.w, e.h});
    SlotGeometry s;
    if (!slotOf(items, kHostCols, kHostRows, &s)) return false;
    return g.cols == s.cols && g.rows == s.rows && g.cellW == s.w * kCellPx &&
           g.cellH == s.h * kCellPx;
}

}  // namespace

bool parseGroupsText(const std::string& text, std::vector<GroupList>* out, std::string* error) {
    out->clear();
    std::vector<std::string> f;
    std::size_t pos = 0;
    int lineNo = 0;
    auto fail = [&](const char* why) {
        if (error) *error = "uniq-groups.txt line " + std::to_string(lineNo) + ": " + why;
        out->clear();
        return false;
    };
    while (pos < text.size()) {
        std::size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        std::string line = text.substr(pos, nl - pos);
        pos = nl + 1;
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        splitTabs(line, &f);
        if (f[0] == "G") {
            int n = 0;
            GroupList g;
            if (f.size() != 8 || !toInt(f[7], &n) || f[2].empty() || !toInt(f[3], &g.cols) ||
                !toInt(f[4], &g.rows) || !toInt(f[5], &g.cellW) || !toInt(f[6], &g.cellH))
                return fail("bad G line");
            if (!out->empty()) {
                if (static_cast<int>(out->back().entries.size()) != out->back().declared)
                    return fail("the previous group's E count differs from its G line");
                if (!slotMatches(out->back()))
                    return fail("the previous group's G geometry is not its entries' slot");
            }
            g.label = f[2];
            g.declared = n;
            out->push_back(g);
        } else if (f[0] == "E") {
            GroupEntry e;
            if (out->empty()) return fail("an E line before any G line");
            if (f.size() != 4 || f[1].empty() || !toInt(f[2], &e.w) || !toInt(f[3], &e.h))
                return fail("bad E line (want: E <record> <w> <h>)");
            if (e.w < 1 || e.w > 2 || e.h < 1 || e.h > 5)
                return fail("footprint outside 1..2 x 1..5");
            e.record = f[1];
            out->back().entries.push_back(e);
        } else {
            return fail("unknown line kind");
        }
    }
    if (out->empty()) {
        if (error) *error = "uniq-groups.txt holds no group";
        return false;
    }
    if (static_cast<int>(out->back().entries.size()) != out->back().declared)
        return fail("the last group's E count differs from its G line");
    if (!slotMatches(out->back()))
        return fail("the last group's G geometry is not its entries' slot");
    return true;
}

}  // namespace gdut


