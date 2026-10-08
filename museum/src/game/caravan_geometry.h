// Proven v4.1 caravan frame, inventory-cell and draw-origin arithmetic.
// Control geometry lives exclusively in ui/geometry.h and ui/row_layout.cpp.
#pragma once
#include "backend/window_math.h"

#include <stdio.h>   // _snprintf_s: the label's line

namespace integration {

struct UtRectF {
    float x, y, w, h;
};

inline bool utPadFinite(float v) { return v == v && v > -1.0e7f && v < 1.0e7f; }

inline bool utRectInside(const UtRectF& in, const UtRectF& out, float tol) {
    return in.x >= out.x - tol && in.y >= out.y - tol && in.x + in.w <= out.x + out.w + tol &&
           in.y + in.h <= out.y + out.h + tol;
}

inline bool utRectsMeet(const UtRectF& a, const UtRectF& b) {
    return a.w > 0.0f && a.h > 0.0f && b.w > 0.0f && b.h > 0.0f && a.x < b.x + b.w &&
           b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

inline bool utRectHas(const UtRectF& r, float x, float y) {
    return r.w > 0.0f && r.h > 0.0f && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

inline float utPadFloor(float v) {   // floorf(v) without <math.h> (|v| < 1e7)
    const float t = (float)(long)v;
    return t > v ? t - 1.0f : t;
}
inline float utPadCeil(float v) { return -utPadFloor(-v); }

inline float utPadRound(float v) {   // floorf(v + 0.5f) without <math.h>
    const float t = v + 0.5f;
    int i = (int)t;
    if ((float)i > t) --i;
    return (float)i;
}

// ---- the frame ---------------------------------------------------------------------------------
const int kUtPadGridCols = 16;   // the Transfer sack, forced by its accessor
const int kUtPadGridRows = 15;
const float kUtFrameTol = 1.0f;  // DRAWN px (canvas px): every frame / grid number is drawn px

struct UtFrameIn {
    float originX, originY;   // the page mouse handler's parent origin (canvas px)
    float gridX, gridY;       // page pos + parent origin: the grid's top-left (canvas px)
    float cellW, cellH;       // the sack's cell size (canvas px)
    float scale;              // Engine::GetUIScale
    float canvasW, canvasH;
    float pageX, pageY;       // record: the Transfer page inside the caravan window (0, 126)
    float frameW, frameH;     // record: the caravan window (565 x 637)
};

enum {
    kUtFrameOk = 1,
    kUtFrameBadInput = -1,     // a NaN, a scale outside (0.3, 4), an empty cell or canvas
    kUtFrameOffCanvas = -2,    // the derived frame leaves the canvas
    kUtFrameGridOutside = -3,  // the measured grid is not inside the derived frame
};

inline int utFrameFromHover(const UtFrameIn& in, UtRectF* frame, UtRectF* grid) {
    const float s = in.scale;
    if (!utPadFinite(in.originX) || !utPadFinite(in.originY) || !utPadFinite(in.gridX) ||
        !utPadFinite(in.gridY) || !utPadFinite(s) || !(s > 0.3f && s < 4.0f) ||
        !(in.cellW > 0.0f && in.cellW < 1000.0f) || !(in.cellH > 0.0f && in.cellH < 1000.0f) ||
        !(in.canvasW > 0.0f) || !(in.canvasH > 0.0f) || !(in.frameW > 0.0f) ||
        !(in.frameH > 0.0f) || !utPadFinite(in.pageX) || !utPadFinite(in.pageY))
        return kUtFrameBadInput;
    const UtRectF f = {in.originX - in.pageX * s, in.originY - in.pageY * s, in.frameW * s,
                       in.frameH * s};
    const UtRectF g = {in.gridX, in.gridY, (float)kUtPadGridCols * in.cellW,
                       (float)kUtPadGridRows * in.cellH};
    if (frame) *frame = f;
    if (grid) *grid = g;
    const UtRectF canvas = {0.0f, 0.0f, in.canvasW, in.canvasH};
    if (!utRectInside(f, canvas, kUtFrameTol)) return kUtFrameOffCanvas;
    if (!utRectInside(g, f, kUtFrameTol)) return kUtFrameGridOutside;
    return kUtFrameOk;
}

inline const char* utFrameWhyText(int r) {
    return r == kUtFrameOk            ? "ok"
           : r == kUtFrameOffCanvas   ? "the derived frame leaves the canvas"
           : r == kUtFrameGridOutside ? "the measured grid is not inside the derived frame"
                                      : "a number is not usable (NaN, scale, cell or canvas)";
}

// ---- the frame follows the window ----------------------------------------------
// A decided frame (accepted or refused) stands while the proven hovers report the grid origin it was
// decided on (within kUtFrameTol); a hover whose origin moved derives it again. More than
// kUtFrameMovesMax moves in one showing of the page = the window will not hold still: refused until
// the page is shown again (no pad and no label while ON). A non-finite origin is no evidence.
const int kUtFrameMovesMax = 16;
struct UtFrameTrack {
    int verdict;          // 0 undecided, 1 accepted, -1 refused
    float gridX, gridY;   // the grid origin the verdict was decided on
    int moves;            // derivations after the first, in this showing
    bool unstable;
};
enum { kUtFrameTrackKeep = 0, kUtFrameTrackDerive = 1, kUtFrameTrackUnstable = 2 };

inline void utFrameTrackReset(UtFrameTrack* t) {
    t->verdict = 0;
    t->gridX = 0.0f;
    t->gridY = 0.0f;
    t->moves = 0;
    t->unstable = false;
}

inline bool utFrameMoved(float ax, float ay, float bx, float by) {
    const float dx = ax - bx, dy = ay - by;
    return !(dx <= kUtFrameTol && dx >= -kUtFrameTol && dy <= kUtFrameTol && dy >= -kUtFrameTol);
}

inline int utFrameTrackStep(UtFrameTrack* t, float gridX, float gridY) {
    if (t->unstable) return kUtFrameTrackKeep;
    if (t->verdict == 0) return kUtFrameTrackDerive;
    if (!utPadFinite(gridX) || !utPadFinite(gridY)) return kUtFrameTrackKeep;
    if (!utFrameMoved(gridX, gridY, t->gridX, t->gridY)) return kUtFrameTrackKeep;
    if (t->moves >= kUtFrameMovesMax) {
        t->unstable = true;
        t->verdict = -1;
        return kUtFrameTrackUnstable;
    }
    ++t->moves;
    return kUtFrameTrackDerive;
}

inline void utFrameTrackDecided(UtFrameTrack* t, int verdict, float gridX, float gridY) {
    t->verdict = verdict;
    t->gridX = gridX;
    t->gridY = gridY;
}

// ---- (rule): ONE drawn cell for the shown page ---------------------------------
// TQ's InventorySack keeps its cell in DRAWN px: its ctor and InventorySack::OnUIScaleChange
// (Game.dll 0x1B6220, the sack is an options listener) set it with 0x198C40 = floorf(32 x
// Engine::GetUIScale + 0.5f) (when not downsizing) and rescale every item rect by new / old. The
// Transfer page's grid is 16 x 15 of that cell; the mouse point the page handler passes to
// GetItemUnderPoint (0x1B6580: x <= px < x + w on the item's sack rect, no division, no scaling) is
// in the same px. So the drawn cell = the measured grid's width / 16, and it must be the mod
// sack's live cell (1 px); the engine's own formula is the third witness.
inline float utEngineCellPx(float scale) { return utPadRound(32.0f * scale); }

enum { kUtCellOk = 0, kUtCellBadInput = -1, kUtCellGridVsSack = -2, kUtCellNotSquare = -3 };
inline int utDrawnCell(float gridW, float gridH, float sackW, float sackH, float* cellPx) {
    if (!utPadFinite(gridW) || !utPadFinite(gridH) || !(gridW > 0.0f) || !(gridH > 0.0f) ||
        !utPadFinite(sackW) || !utPadFinite(sackH) || !(sackW >= 4.0f) || !(sackH >= 4.0f))
        return kUtCellBadInput;
    const float cw = gridW / (float)kUtPadGridCols, ch = gridH / (float)kUtPadGridRows;
    const float dw = cw - sackW, dh = ch - sackH;
    if (dw > kUtFrameTol || dw < -kUtFrameTol || dh > kUtFrameTol || dh < -kUtFrameTol)
        return kUtCellGridVsSack;
    if (cw - ch > kUtFrameTol || ch - cw > kUtFrameTol) return kUtCellNotSquare;
    if (cellPx) *cellPx = cw;
    return kUtCellOk;
}
inline const char* utDrawnCellWhyText(int r) {
    return r == kUtCellOk            ? "ok"
           : r == kUtCellGridVsSack  ? "the grid / 16 is not the mod sack's cell"
           : r == kUtCellNotSquare   ? "the grid's cell is not square"
                                     : "a number is not usable";
}

inline UtRectF utRectInset(const UtRectF& r, float k) {
    const UtRectF o = {r.x + k, r.y + k, r.w - 2.0f * k, r.h - 2.0f * k};
    return o;
}

// ONE helper: a slot (col, row, w, h in cells) -> its DRAWN rect on the grid (canvas px). Every
// cells-to-px turn of the draw side (plates, slot art, grid cover, ring, veils) goes through it.
inline UtRectF utSlotDrawnRect(const UtRectF& grid, int col, int row, int w, int h) {
    const float cw = grid.w / (float)kUtPadGridCols, ch = grid.h / (float)kUtPadGridRows;
    const UtRectF r = {grid.x + (float)col * cw, grid.y + (float)row * ch, (float)w * cw,
                       (float)h * ch};
    return r;
}
// The search's highlight on one slot (drawn px). The frame lies on the slot's OUTERMOST pixel
// ring (inset 0: clear of the engine's rarity art inside the slot, never in the next slot), 1 px
// thick below a 48 px cell (UI scale under 1.5), 2 px from there. The wash fills the slot inset by
// 1 px, under the icon.
inline float utSearchMarkThick(float cellPx) { return cellPx >= 48.0f ? 2.0f : 1.0f; }
inline UtRectF utSearchMarkRect(const UtRectF& slot, bool wash) {
    return wash ? utRectInset(slot, 1.0f) : slot;
}
// The real Transfer page (the view OFF): one item's rect in the real sack (InventorySack's
// RectExt: drawn px from the grid's top-left, the unit GetItemUnderPoint compares the page point
// with) -> canvas px, both edges rounded. False = not a number, empty, or leaving the 16 x 15 grid
// by more than half a pixel: such an item is never marked.
inline bool utSearchRealRect(const UtRectF& grid, float x, float y, float w, float h, UtRectF* out) {
    if (!utPadFinite(x) || !utPadFinite(y) || !utPadFinite(w) || !utPadFinite(h) ||
        !utPadFinite(grid.x) || !utPadFinite(grid.y) || !(grid.w > 0.0f) || !(grid.h > 0.0f) ||
        !(w >= 1.0f) || !(h >= 1.0f) || x < -0.5f || y < -0.5f || x + w > grid.w + 0.5f ||
        y + h > grid.h + 0.5f)
        return false;
    const float x0 = utPadRound(grid.x + x), y0 = utPadRound(grid.y + y);
    const float x1 = utPadRound(grid.x + x + w), y1 = utPadRound(grid.y + y + h);
    if (out) *out = UtRectF{x0, y0, x1 - x0, y1 - y0};
    return true;
}
// ... and a point on the page (px from the grid's top-left, the handler's point) -> its cell.
// -1 when outside the 16 x 15 grid or not a number.
inline bool utCellAtPoint(float px, float py, float cellPx, int* col, int* row) {
    if (!(cellPx >= 4.0f) || !(px >= 0.0f) || !(py >= 0.0f) || !(px < 1.0e6f) || !(py < 1.0e6f))
        return false;
    const int c = (int)(px / cellPx), r = (int)(py / cellPx);
    if (c >= kUtPadGridCols || r >= kUtPadGridRows) return false;
    if (col) *col = c;
    if (row) *row = r;
    return true;
}

// Before the first measurement (and after a refused one, view OFF): the Collection toggle alone,
// 3 x 32 + 2 record px wide, centred on the RECORD frame's middle, its bottom 10 record px above
// the record frame's bottom. The records put the frame 56 px left of where the engine drew it on
// a measured 1366 x 768 window, so only a small centred button is safe: it stays inside the
// live frame for any horizontal error below ~230 px, far from the character window.
// the caravan frame from the Transfer page DRAW (TQ.exe 0xC31A0), so the
// pad is there from the first frame the tab is shown. The draw's own arithmetic:
//   pos = the page's own place, floats [page+0x1C] / [page+0x20]
//   out = 0x176750(pos): pos x Engine::GetUIScale() when GraphicsEngine::IsDownsizing() is false
//         (0x1767E3), a width/height-based scale when it is true
//   pass != 1: the inventory is drawn at (origin.x + pos.x, origin.y + out.y)   (0xC3229..0xC3241)
//   pass == 1: its y is out.y x 0x176840 (a second, height-based factor)
// That point is the page's parent origin as the page MOUSE HANDLER receives it (the earlier reading,
// the same number utFrameFromHover subtracts the page's record place from), and the grid is it plus
// the inventory's own place [UIStashInventory+0x10/+0x14] (the handler's `F3 0F 10 5B 10`). Refused
// (the hover route stays the fallback) when downsizing is true or unknown, in pass 1, or when a
// number is not finite or not a plausible canvas coordinate.
enum { kUtDrawOk = 0, kUtDrawDownsizing, kUtDrawPass1, kUtDrawBadInput };
struct UtDrawOriginIn {
    float originX, originY;   // the draw's origin argument ([ebp+0xC] -> 2 floats)
    float posX, posY;         // [page+0x1C] / [page+0x20]
    float invX, invY;         // [UIStashInventory+0x10] / [+0x14]
    float uiScale;            // Engine::GetUIScale
    int pass;                 // the draw's pass argument
    bool downsizingKnown, downsizing;
};
inline int utDrawOrigin(const UtDrawOriginIn& in, float* pageX, float* pageY, float* gridX,
                        float* gridY) {
    if (!in.downsizingKnown || in.downsizing) return kUtDrawDownsizing;
    if (in.pass == 1) return kUtDrawPass1;
    const float lim = 20000.0f;
    const float v[7] = {in.originX, in.originY, in.posX, in.posY, in.invX, in.invY, in.uiScale};
    for (int i = 0; i < 7; ++i)
        if (!utPadFinite(v[i]) || v[i] < -lim || v[i] > lim) return kUtDrawBadInput;
    if (!(in.uiScale > 0.3f && in.uiScale < 4.0f)) return kUtDrawBadInput;
    const float px = in.originX + in.posX;             // x: the raw place (0xC322D)
    const float py = in.originY + in.posY * in.uiScale;   // y: 0x176750's out.y (0xC323C)
    if (pageX) *pageX = px;
    if (pageY) *pageY = py;
    if (gridX) *gridX = px + in.invX;
    if (gridY) *gridY = py + in.invY;
    return kUtDrawOk;
}
inline const char* utDrawWhyText(int r) {
    return r == kUtDrawOk           ? "ok"
           : r == kUtDrawDownsizing ? "GraphicsEngine::IsDownsizing is set or unknown"
           : r == kUtDrawPass1      ? "the pass-1 draw (a second, height-based factor)"
                                    : "a number is not usable";
}
// A draw-derived frame is taken only after the same page point held for this many frames in a row
// (a window that slides in never feeds a moving point to the frame tracker).
const int kUtDrawStableFrames = 3;

// the Ctrl+wheel / Ctrl+PgUp / Ctrl+PgDn cycle in the pad's own order:
// Transfer, G1 ... Gn, Transfer ... `from` = the group shown (0-based), or kUtCycleTransfer for the
// real Transfer page (the view OFF); dir -1 = back (wheel up, PgUp), +1 = forward (wheel down,
// PgDn). One step per event. Returns the target group, kUtCycleTransfer, or kUtCycleNone (no groups).
// the wheel's notch accumulator - the usual remainder rule. A wheel, a
// touchpad or a keyboard that reports deltas smaller than one notch (WHEEL_DELTA = 120) adds up
// until |sum| reaches a notch: the whole notches are answered (signed, + = wheel up) and the
// remainder is kept; a delta in the other direction drops the remainder first. ut_panel keeps one
// for the Ctrl cycle and one for the plain wheel (a switch between the two resets the other).
struct UtWheelAcc {
    int sum = 0;
};
inline int utWheelAccumulate(UtWheelAcc& a, int delta, int notch = 120) {
    if (delta == 0 || notch <= 0) return 0;
    if (a.sum != 0 && ((a.sum > 0) != (delta > 0))) a.sum = 0;   // a direction change
    long long s = (long long)a.sum + (long long)delta;
    const long long cap = 1000LL * notch;   // no overflow from a flood of deltas
    if (s > cap) s = cap;
    if (s < -cap) s = -cap;
    const long long n = s / notch;   // toward zero: the remainder keeps the sum's sign
    a.sum = (int)(s - n * notch);
    return (int)n;
}

// ---- the slot-wide item tint - pure -----------------------------------
// The inventory draw (TQ.exe 0x16E300, the Transfer page draw's `FF 50 0C`) gives every item widget
// a background through 0x10A9B0 -> 0x10A850, at the item's FOOTPRINT inset by the inventory's
// [+0x134] px. What 0x10A850 draws (disassembled, section 2):
//   requirements NOT met (EquipmentCtrl::AreRequirementsMet false)  -> the inventory's
//       failsRequirementsColor [+0x124] (stashinventory.dbr 0.5, 0, 0, 0.5)             = Fails
//   met, the item-background option on (Options::GetBool(0x18)), Item::GetActualItemClassification
//       != 0 and GameEngine::GetItemColor(cls) answers -> that colour with
//       GameEngine::GetItemBackgroundOpacity(hover) as alpha (then its border texture)  = Class
//   met otherwise -> the inventory's backgroundShadeColor [+0x114] (0.5, 0.5, 0.5, 0.3)  = Shade
//       (the engine's hovered widget draws 0.4, 0.6, 0.8, 0.3 instead - not reproduced)
enum { kUtTintNone = 0, kUtTintFails = 1, kUtTintClass = 2, kUtTintShade = 3 };
inline int utTintKind(bool met, bool optionOn, int cls, bool colourOk) {
    if (!met) return kUtTintFails;
    if (optionOn && cls != 0 && colourOk) return kUtTintClass;
    return kUtTintShade;
}

// The RING between the item's tinted rect and its slot's (both already inset by the engine's
// inset): up to four rectangles - the band above the item, the band below it (both the slot's
// whole width), and the parts left and right of it (the item's height). The item is clipped to the
// slot first; an item that fills the slot gives none, an item outside the slot the whole slot. The
// rectangles never overlap the item's rect (the icon is inside it), nor each other. Slivers under
// 0.01 px are dropped. Returns the count.
inline int utSlotRing(const UtRectF& slot, const UtRectF& item, UtRectF out[4]) {
    const float eps = 0.01f;
    if (!(slot.w > eps && slot.h > eps)) return 0;
    const float sx1 = slot.x + slot.w, sy1 = slot.y + slot.h;
    float ix0 = item.x > slot.x ? item.x : slot.x, iy0 = item.y > slot.y ? item.y : slot.y;
    float ix1 = item.x + item.w < sx1 ? item.x + item.w : sx1;
    float iy1 = item.y + item.h < sy1 ? item.y + item.h : sy1;
    int n = 0;
    if (!(ix1 - ix0 > eps && iy1 - iy0 > eps)) {   // no overlap: the whole slot
        out[n++] = slot;
        return n;
    }
    if (iy0 - slot.y > eps) out[n++] = UtRectF{slot.x, slot.y, slot.w, iy0 - slot.y};
    if (sy1 - iy1 > eps) out[n++] = UtRectF{slot.x, iy1, slot.w, sy1 - iy1};
    if (ix0 - slot.x > eps) out[n++] = UtRectF{slot.x, iy0, ix0 - slot.x, iy1 - iy0};
    if (sx1 - ix1 > eps) out[n++] = UtRectF{ix1, iy0, sx1 - ix1, iy1 - iy0};
    return n;
}

}  // namespace integration
