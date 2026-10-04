// Native game surface and MuseumPanel bridge; v4.1 hook contracts are retained.
#pragma once

#include <windows.h>

#include "game/caravan_geometry.h"

namespace integration {

void panelDraw();

bool panelInput(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
bool panelSearchFieldShown();

void panelForgetFonts();

void panelGrayPrepare(bool noWorldYet);

bool panelCursorNow(float* x, float* y);

void panelNoteHover(float originX, float originY, float gridX, float gridY, unsigned cellW,
                    unsigned cellH);
bool panelFrame(UtRectF* frame);
bool panelFrameGrid(UtRectF* grid);
bool panelCellPx(float* cell);
int panelCellVerdict(float* cell);
bool panelSlotGrid(UtRectF* grid);
const char* panelFrameSourceText();

const char* panelStatus();

bool panelPadClaimsPress();
void panelPageDrawPre(void* page, void* canvas, const void* origin, int pass);
void panelPageDrawPost(void* page, void* canvas, int pass);
void panelRectRouteBegin(void* page);
void panelRectRouteEnd();
void panelRectRouteUnwound();
int panelItemBackgroundPre(void* widget);
void panelItemBackgroundPost(int save);   // utBgTokenSave(token) >= 0

}  // namespace integration
