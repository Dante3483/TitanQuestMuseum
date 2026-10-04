#pragma once
#include <vector>
#include <string>
namespace gdut {
struct PackItem {
    int index = 0;   // the caller's handle (the entry index inside its group)
    int w = 1;       // footprint in cells
    int h = 1;
};

// A group's slot: its size in cells and how many fit on a page.
struct SlotGeometry {
    int w = 0, h = 0;          // the largest footprint of the group
    int cols = 0, rows = 0;    // slots per page across / down
    int perPage() const noexcept { return cols * rows; }
};

struct PackPlacement {
    int index = 0;
    int page = 0;
    int col = 0;       // the footprint's top-left cell (centred inside its slot)
    int row = 0;
    int slotCol = 0;   // the slot's top-left cell (slot size = the group's SlotGeometry)
    int slotRow = 0;
};

// Spread the existing slot rows over the host's integer cell rows. Footprints,
// slot count and navigation stay unchanged; all consumers use these same bounds.
inline int filledRowEdge(const SlotGeometry& g, int row, int hostRows) noexcept {
    const int used = g.rows * g.h;
    if (used <= 0 || row <= 0) return 0;
    if (row >= used) return hostRows;
    // Round each boundary to the nearest cell, spreading spare height across rows.
    return (row * hostRows + used / 2) / used;
}
inline void fillWindowSlot(const SlotGeometry& g, int hostRows, int itemH,
                           PackPlacement* p, int* height) noexcept {
    const int top = filledRowEdge(g, p->slotRow, hostRows);
    const int bottom = filledRowEdge(g, p->slotRow + g.h, hostRows);
    p->slotRow = top;
    *height = bottom - top;
    p->row = top + (*height - itemH) / 2;
}

inline constexpr int kHostCols = 16;   // the Transfer sack, forced by its page accessor
inline constexpr int kHostRows = 15;
inline constexpr int kCellPx = 32;     // uniq-groups.txt's G line gives the slot in 32 px cells

// The slot of `items` on a cols x rows page. False when there is no item or a footprint is not
// legal (w/h < 1, or larger than the page).
bool slotOf(const std::vector<PackItem>& items, int cols, int rows, SlotGeometry* out);

// Packs `items` in order into uniform slots (see above). Returns the page count (0 for no items),
// or -1 when a footprint is not legal - nothing is placed then. `slot` (optional) gets the slot.
int packSlots(const std::vector<PackItem>& items, int cols, int rows,
              std::vector<PackPlacement>* out, SlotGeometry* slot = nullptr);

// the place of the record shown as number `ordinal` (0-based, in list order among the
// records SHOWN) in slot geometry `g`: page = ordinal / perPage, the slot in reading order, the
// footprint fw x fh centred in it. packSlots is this over every record; the OWN filter is this
// over the owned ones only. False when `g` is empty, the ordinal negative or the footprint
// larger than the slot.
bool placeInSlot(const SlotGeometry& g, int ordinal, int index, int fw, int fh,
                 PackPlacement* out) noexcept;

// (GD's owned-only view, "the OWN filter"): the records of ONE group shown on `page`, in
// list order, in the GROUP's slot `g` (never the subset's: the boxes keep their size when the
// filter is on). `show` null = every record; else only the records whose flag is non-zero are
// shown, packed into consecutive slots. Writes at most `cap` placements for `page` into `out`
// and returns how many; `*shown` (optional) gets the number of records shown over ALL pages.
// No allocation: the DLL calls it at every page build.
int placePage(const SlotGeometry& g, const unsigned char* fw, const unsigned char* fh, int n,
              const unsigned char* show, int page, PackPlacement* out, int cap,
              int* shown = nullptr) noexcept;

// Pages for `shown` records in geometry `g`: ceil(shown / perPage), and at least 1 (an empty
// owned-only view is ONE empty page, never zero pages). 0 when `g` is empty.
int pagesFor(const SlotGeometry& g, int shown) noexcept;

// The page window, so that scrolling moves by slot rows and non-last pages always take the
// full tab grid: the view shows a WINDOW of `g.rows` slot rows starting at slot row
// `rowOffset` of the group's records laid out in reading order (`g.cols` slots a row). The wheel
// moves the offset by one slot row, `<` `>` / PgUp / PgDn by a window. The offset is clamped to
// 0 .. max(0, slotRows - g.rows), so the window stays FULL whenever the group has at least g.rows
// slot rows (the last window overlaps the one before it); a shorter group shows from row 0.
//
// slot rows needed for `shown` records: ceil(shown / cols); 0 for none or an empty geometry.
int slotRowsFor(const SlotGeometry& g, int shown) noexcept;
// The largest legal offset: max(0, slotRowsFor - g.rows).
int maxRowOffset(const SlotGeometry& g, int shown) noexcept;
// `offset` clamped to 0 .. maxRowOffset.
int clampRowOffset(const SlotGeometry& g, int shown, int offset) noexcept;
// placePage for a window: the shown records whose slot row is inside [rowOffset, rowOffset +
// g.rows), placed as if the window were page 0 (slotRow counts from the window's top). The offset
// is used as given (clamp it first). Same contract as placePage otherwise; `page` is 0.
int placeWindow(const SlotGeometry& g, const unsigned char* fw, const unsigned char* fh, int n,
                const unsigned char* show, int rowOffset, PackPlacement* out, int cap,
                int* shown = nullptr) noexcept;

// A rectangle of host cells (col, row, w, h), e.g. a leftover area of the 16 x 15 grid.
struct CellRect {
    int col = 0, row = 0, w = 0, h = 0;
};
// The cells of a hostCols x hostRows grid that NO slot of the window holding a record uses: at
// most three disjoint rectangles, in this order - the strip right of the full slot rows when
// cols x w < hostCols, the cells after the last record of a partial last row (to the right edge,
// one slot high), and every cell row under the last used slot row (the whole width). An empty
// window gives the whole grid; a window whose slots fill the grid gives none. Returns the count.
int windowLeftover(const SlotGeometry& g, int shown, int rowOffset, int hostCols, int hostRows,
                   CellRect out[3]) noexcept;

// One group of uniq-groups.txt as writes it: `G <i> <label> <cols> <rows> <cw> <ch> <n>`
// then n lines `E <record> <w> <h>` (tab separated, CRLF or LF). the G line's geometry IS
// the slot - cols x rows slots per page, each cw x ch pixels (32 px cells) - and it must equal the
// slot the E lines give (their largest footprint); the parser refuses a group where it does not.
struct GroupEntry {
    std::string record;
    int w = 1;
    int h = 1;
};
struct GroupList {
    std::string label;
    int declared = 0;               // the G line's count
    int cols = 0, rows = 0;         // the G line's slots per page across / down
    int cellW = 0, cellH = 0;       // the G line's slot size in pixels
    std::vector<GroupEntry> entries;
};

// Parses the whole text. False (and *error) on a malformed line, an E before any G, a footprint
// outside 1..2 x 1..5, a group whose E count differs from its G line, or a G line whose geometry is
// not its entries' slot: refuse, never guess.
bool parseGroupsText(const std::string& text, std::vector<GroupList>* out,
                     std::string* error = nullptr);

}  // namespace gdut

