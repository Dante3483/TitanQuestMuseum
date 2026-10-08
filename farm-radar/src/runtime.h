#pragma once
#include <windows.h>
#include <cstddef>
#include <map>
#include <string>
namespace integration {
struct TqPtrVector {const void** first;const void** last;const void** end;};
static_assert(sizeof(TqPtrVector)==12,"engine x86 vector ABI");
struct GameApi {
    void*(__cdecl* ObjectManagerGet)()=nullptr;
    void(__thiscall* ObjectManagerGetObjectList)(const void*,TqPtrVector*)=nullptr;
    const char*(__thiscall* ObjectGetObjectName)(const void*)=nullptr;
    void(__cdecl* CrtOperatorDelete)(void*)=nullptr;
};
struct RadarConfig {int enabled=1,farmRadar=1,farmRadarHotkey=119,farmRadarRadius=30;};
extern GameApi g_tq;extern RadarConfig g_cfg;
bool safeRead(const void*,void*,size_t);
void logI(const char* format,...);
void logW(const char* format,...);
const void* radarLocalPlayer();
int radarCanvasWidth();int radarCanvasHeight();
const std::map<std::string,std::string>& tooltipFarmTargets();
unsigned tooltipSourceRevision();
bool searchFieldFocused();bool panelViewerActive();
}
