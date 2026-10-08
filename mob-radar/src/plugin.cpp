#include <windows.h>
#include <cstdio>
#include <share.h>
#include <cstdarg>
#include <cstring>
#include <algorithm>
#include <vector>
#include <cwchar>
#include "tqt_core.h"
#include "runtime.h"
#include "farm_radar.h"
#include "tqt_radar.h"

namespace integration {
GameApi g_tq;RadarConfig g_cfg;
namespace {
HMODULE selfModule=nullptr;FILE* logfile=nullptr;SRWLOCK logLock=SRWLOCK_INIT;
wchar_t iniPath[MAX_PATH]={};
const TqmAddonApiV1* host=nullptr;
const TqtCoreApiV2* coreProvider=nullptr;volatile LONG registered=0;
TqtRadarInspectorV1 inspector={};SRWLOCK inspectorLock=SRWLOCK_INIT;
const void* localPlayer=nullptr;int canvasWidth=0,canvasHeight=0;
bool keyboardBusy=false,sourcesReady=false;
unsigned sourceRevision=0,appliedRevision=0;
std::map<std::string,std::string> targets;
void logV(const char* level,const char* format,va_list args){
    AcquireSRWLockExclusive(&logLock);
    if(logfile){std::fprintf(logfile,"%s ",level);std::vfprintf(logfile,format,args);std::fputc('\n',logfile);std::fflush(logfile);}
    ReleaseSRWLockExclusive(&logLock);
}
template<class T> bool bind(T& target,HMODULE module,const char* name){
    target=reinterpret_cast<T>(GetProcAddress(module,name));if(!target)logW("missing export: %s",name);return target!=nullptr;
}
bool bindGame(){
    HMODULE e=GetModuleHandleW(L"Engine.dll"),crt=GetModuleHandleW(L"MSVCR110.dll");
    bool ok=true;
    ok=bind(g_tq.ObjectManagerGet,e,"?Get@?$Singleton@VObjectManager@GAME@@@GAME@@SAPAVObjectManager@2@XZ")&&ok;
    ok=bind(g_tq.ObjectManagerGetObjectList,e,"?GetObjectList@ObjectManager@GAME@@QBEXAAV?$vector@PBVObject@GAME@@V?$allocator@PBVObject@GAME@@@std@@@std@@@Z")&&ok;
    ok=bind(g_tq.ObjectGetObjectName,e,"?GetObjectName@Object@GAME@@QBEPBDXZ")&&ok;
    ok=bind(g_tq.CrtOperatorDelete,crt,"??3@YAXPAX@Z")&&ok;
    return ok;
}
void configTick(){
    static DWORD last=0;const DWORD now=GetTickCount();if(last && now-last<1000)return;last=now;
    const int enabled=int(GetPrivateProfileIntW(L"radar",L"enabled",1,iniPath));
    const int key=int(GetPrivateProfileIntW(L"radar",L"hotkey",119,iniPath));
    const int radius=int(GetPrivateProfileIntW(L"radar",L"radius",30,iniPath));
    g_cfg.farmRadar=enabled?1:0;g_cfg.farmRadarHotkey=(std::max)(0,(std::min)(254,key));
    g_cfg.farmRadarRadius=(std::max)(1,(std::min)(200,radius));
}
bool refreshTargets(const TqmFrameV1* frame){
    sourceRevision=frame->sourceRevision;
    if(!(frame->flags&TQM_FRAME_SOURCES_READY)){sourcesReady=false;targets.clear();return true;}
    if(sourcesReady && appliedRevision==sourceRevision)return true;
    uint32_t count=0;int status=host->copyFarmTargets(sourceRevision,nullptr,0,&count);
    if(status!=TQM_COPY_OK || count>16384){sourcesReady=false;targets.clear();return false;}
    std::vector<TqmFarmTargetV1> copied(count);
    if(count){status=host->copyFarmTargets(sourceRevision,copied.data(),count,&count);
        if(status!=TQM_COPY_OK){sourcesReady=false;targets.clear();return false;}}
    std::map<std::string,std::string> next;
    for(const auto& row:copied){
        if(!std::memchr(row.record,0,sizeof(row.record)) || !std::memchr(row.name,0,sizeof(row.name)) ||
           std::strncmp(row.record,"records/",8)!=0 || !row.name[0]){sourcesReady=false;targets.clear();return false;}
        next.emplace(row.record,row.name);
    }
    targets.swap(next);sourcesReady=true;appliedRevision=sourceRevision;
    return true;
}
class HostRenderer final:public museum::ui::Renderer {
    void* context_;
public:
    explicit HostRenderer(void* context):context_(context){}
    void fill(museum::ui::Rect rect,museum::ui::Color color) override{
        const TqmRectV1 r={rect.x,rect.y,rect.w,rect.h};const TqmColorV1 c={color.r,color.g,color.b,color.a};
        host->fillRect(context_,&r,&c);
    }
    float measure(const wchar_t* text,int size) override{return host->measureText(context_,text,size);}
    void text(museum::ui::Rect rect,const wchar_t* text,int size,museum::ui::Color color,museum::ui::TextAlign align) override{
        const TqmRectV1 r={rect.x,rect.y,rect.w,rect.h};const TqmColorV1 c={color.r,color.g,color.b,color.a};
        host->drawText(context_,&r,text,size,&c,int(align));
    }
};
void TQM_CALL onFrame(void*,const TqmFrameV1* frame) try {
    if(!frame || frame->version!=TQM_ADDON_API_VERSION || frame->size!=sizeof(*frame))return;
    static bool bound=false,attempted=false;
    if(!attempted){attempted=true;bound=bindGame();if(!bound)farmRadarFault("required game exports missing");}
    if(!bound)return;
    configTick();keyboardBusy=(frame->flags&TQM_FRAME_KEYBOARD_BUSY)!=0;
    localPlayer=frame->localPlayer;canvasWidth=frame->canvasWidth;canvasHeight=frame->canvasHeight;
    if(!refreshTargets(frame)){farmRadarFault("Core source snapshot incompatible");return;}
    farmRadarTick();
    if(frame->flags&TQM_FRAME_DRAW_AVAILABLE){HostRenderer renderer(frame->renderContext);farmRadarDraw(renderer,canvasWidth,canvasHeight);}
    localPlayer=nullptr; // never expose a borrowed player outside the frame callback
} catch(...){localPlayer=nullptr;farmRadarFault("frame exception");}
int32_t TQM_CALL onMessage(void*,void* hwnd,uint32_t message,uintptr_t wp,intptr_t lp,uint32_t busy) try {
    keyboardBusy=busy!=0;return farmRadarInput(static_cast<HWND>(hwnd),message,wp,lp)?1:0;
} catch(...){farmRadarFault("input exception");return 0;}
int32_t TQM_CALL onKey(void*,int32_t button,int32_t state,uint32_t busy){
    keyboardBusy=busy!=0;return farmRadarKeyGate(button,state)?1:0;
}
bool prepareFiles(){
    wchar_t path[MAX_PATH]={};if(!GetModuleFileNameW(selfModule,path,MAX_PATH))return false;
    wchar_t* slash=std::wcsrchr(path,L'\\');if(!slash)return false;slash[1]=0;
    wchar_t folder[MAX_PATH]={};if(swprintf_s(folder,L"%sTitanQuestMobRadar",path)<0)return false;
    if(!CreateDirectoryW(folder,nullptr) && GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    if(swprintf_s(iniPath,L"%s\\TitanQuestMobRadar.ini",folder)<0)return false;
    wchar_t logPath[MAX_PATH]={};if(swprintf_s(logPath,L"%s\\TitanQuestMobRadar.log",folder)<0)return false;
    logfile=_wfsopen(logPath,L"w",_SH_DENYNO);
    wchar_t legacy[MAX_PATH]={};
    if(swprintf_s(legacy,L"%sTitanQuestFarmRadar\\TitanQuestFarmRadar.ini",path)>=0)CopyFileW(legacy,iniPath,TRUE);
    HANDLE file=CreateFileW(iniPath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file!=INVALID_HANDLE_VALUE){
        const char defaults[]="; TitanQuestMobRadar, dependent Core add-on\r\n; F8 toggles for this game session. Edits reload once per second.\r\n[radar]\r\nenabled=1\r\nhotkey=119\r\nradius=30\r\n";
        DWORD written=0;WriteFile(file,defaults,sizeof(defaults)-1,&written,nullptr);CloseHandle(file);
    }
    return true;
}
DWORD WINAPI initialize(void*){
    if(!prepareFiles())return 0;
    logI("TitanQuestMobRadar 0.5, x86: waiting for TitanQuestCore API v2");
    for(unsigned i=0;i<600;++i){
        HMODULE provider=GetModuleHandleW(L"TitanQuestCore.asi");
        if(provider){auto getApi=reinterpret_cast<TqtGetCoreApi>(GetProcAddress(provider,"TQT_GetCoreApi"));
            if(!getApi){logW("OFF: Core build lacks add-on API; install the matching Core ASI");return 0;}
            const auto* core=getApi(TQT_CORE_API_VERSION);
            if(!core||core->size!=sizeof(*core)||core->version!=TQT_CORE_API_VERSION){logW("OFF: incompatible Core API");return 0;}
            host=core->addons;
            if(!host || host->size!=sizeof(*host) || host->version!=TQM_ADDON_API_VERSION ||
               !host->registerCallbacks || !host->copyFarmTargets || !host->fillRect || !host->measureText || !host->drawText){
                logW("OFF: incompatible Core add-on API");return 0;}
            // Keep the provider's function table valid for the process lifetime as well.
            HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<const wchar_t*>(getApi),&pinned);
            const TqmCallbacksV1 callbacks={sizeof(TqmCallbacksV1),TQM_ADDON_API_VERSION,nullptr,onFrame,onMessage,onKey};
            const unsigned token=host->registerCallbacks(&callbacks);
            if(!token){logW("OFF: Core rejected add-on registration");return 0;}
            coreProvider=core;InterlockedExchange(&registered,1);
            logI("ON: subscribed to Core frame/input callbacks, registration %u",token);return 0;
        }
        Sleep(200);
    }
    logW("OFF: TitanQuestCore.asi dependency is not loaded");return 0;
}
} // namespace
bool safeRead(const void* from,void* to,size_t size){
    __try {std::memcpy(to,from,size);return true;} __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void logI(const char* format,...){va_list args;va_start(args,format);logV("I",format,args);va_end(args);}
void logW(const char* format,...){va_list args;va_start(args,format);logV("W",format,args);va_end(args);}
const void* radarLocalPlayer(){return localPlayer;}
int radarCanvasWidth(){return canvasWidth;}int radarCanvasHeight(){return canvasHeight;}
const std::map<std::string,std::string>& tooltipFarmTargets(){return targets;}
unsigned tooltipSourceRevision(){return sourceRevision;}
bool searchFieldFocused(){return keyboardBusy;}bool panelViewerActive(){return keyboardBusy;}
int32_t TQM_CALL radarReady(){return InterlockedCompareExchange(&registered,0,0)&&coreProvider&&coreProvider->isReady();}
int32_t TQM_CALL radarCollection(const TqtCollectedItemV1* rows,uint32_t count,uint32_t known){
    return radarReady()?coreProvider->setCollection(rows,count,known):0;
}
int32_t TQM_CALL radarDirectory(char* output,uint32_t capacity){return coreProvider?coreProvider->copyDataDirectory(output,capacity):0;}
int32_t TQM_CALL radarRegisterInspector(const TqtRadarInspectorV1* incoming){
    TqtRadarInspectorV1 copy={};
    if(!incoming||!safeRead(incoming,&copy,sizeof(copy))||copy.size!=sizeof(copy)||!copy.inspect)return 0;
    HMODULE pinned=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<const wchar_t*>(copy.inspect),&pinned))return 0;
    AcquireSRWLockExclusive(&inspectorLock);
    const bool accepted=!inspector.inspect;
    if(accepted)inspector=copy;
    ReleaseSRWLockExclusive(&inspectorLock);
    if(accepted)logI("Museum inventory inspector connected");return accepted?1:0;
}
bool radarHasInspector(){AcquireSRWLockShared(&inspectorLock);const bool active=inspector.inspect!=nullptr;ReleaseSRWLockShared(&inspectorLock);return active;}
uint32_t radarInspect(const TqtRadarScanV1* scan,TqtRadarLootV1* output,uint32_t capacity){
    AcquireSRWLockShared(&inspectorLock);const auto copy=inspector;ReleaseSRWLockShared(&inspectorLock);
    if(!copy.inspect)return 0;
    __try {const uint32_t count=copy.inspect(copy.user,scan,output,capacity);return count<=capacity?count:0;}
    __except(EXCEPTION_EXECUTE_HANDLER){
        AcquireSRWLockExclusive(&inspectorLock);inspector={};ReleaseSRWLockExclusive(&inspectorLock);
        logW("Museum inventory inspector disabled after callback fault");return 0;
    }
}
} // namespace integration
#pragma comment(linker,"/EXPORT:TQT_GetMobRadarApi=_TQT_GetMobRadarApi")
extern "C" const void* TQM_CALL TQT_GetMobRadarApi(uint32_t version){
    static const TqtMobRadarApiV1 legacy={sizeof(TqtMobRadarApiV1),1,integration::radarReady,integration::radarCollection};
    static const TqtMobRadarApiV2 api={sizeof(TqtMobRadarApiV2),TQT_RADAR_API_VERSION,integration::radarReady,integration::radarCollection,
        integration::radarDirectory,integration::radarRegisterInspector};
    if(version==1)return &legacy;
    return version==TQT_RADAR_API_VERSION?&api:nullptr;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){integration::selfModule=module;DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,integration::initialize,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
