#pragma once
#include <windows.h>
#include "ui/renderer.h"
namespace integration {
bool addonHostRegistered();
void addonHostFrame(museum::ui::Renderer* renderer,int width,int height,const void* player,bool keyboardBusy);
bool addonHostInput(HWND,UINT,WPARAM,LPARAM,bool keyboardBusy);
bool addonHostKey(const void* event,bool keyboardBusy);
}
