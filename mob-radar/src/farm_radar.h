#pragma once
#include <windows.h>
#include "ui/renderer.h"
namespace integration {
void farmRadarTick();
void farmRadarDraw(museum::ui::Renderer&,int width,int height);
bool farmRadarShown();
bool farmRadarInput(HWND,UINT,WPARAM,LPARAM);
bool farmRadarKeyGate(int button,int state);
void farmRadarForget();
void farmRadarFault(const char* reason);
}
