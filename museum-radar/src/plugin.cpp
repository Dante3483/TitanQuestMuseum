#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cwchar>
#include <share.h>
#include "tqt_radar.h"
#include "collection_reader.h"
#include "runtime.h"
#include "loot_inspector.h"
namespace integration {namespace {
HMODULE selfModule=nullptr;FILE* logFile=nullptr;SRWLOCK logLock=SRWLOCK_INIT;
void logV(const char* level,const char* f,va_list a){AcquireSRWLockExclusive(&logLock);
    if(logFile){std::fprintf(logFile,"%s ",level);std::vfprintf(logFile,f,a);std::fputc('\n',logFile);std::fflush(logFile);}ReleaseSRWLockExclusive(&logLock);}
DWORD WINAPI initialize(void*){
    wchar_t path[MAX_PATH]={};if(!GetModuleFileNameW(selfModule,path,MAX_PATH))return 1;
    wchar_t* slash=std::wcsrchr(path,L'\\');if(!slash)return 1;*slash=0;
    wchar_t folder[MAX_PATH]={},logPath[MAX_PATH]={};swprintf_s(folder,L"%s\\TitanQuestMuseumRadar",path);CreateDirectoryW(folder,nullptr);
    swprintf_s(logPath,L"%s\\TitanQuestMuseumRadar.log",folder);logFile=_wfsopen(logPath,L"w",_SH_DENYNO);
    logI("MuseumRadar 0.2: waiting for MobRadar API v2 and Museum; inventory highlighting enabled");
    for(;;){
        HMODULE mob=GetModuleHandleW(L"TitanQuestMobRadar.asi"),museum=GetModuleHandleW(L"TitanQuestMuseum.asi");
        if(mob&&museum){
            auto get=reinterpret_cast<TqtGetMobRadarApi>(GetProcAddress(mob,"TQT_GetMobRadarApi"));
            const auto* api=get?static_cast<const TqtMobRadarApiV2*>(get(TQT_RADAR_API_VERSION)):nullptr;
            if(!api||api->size!=sizeof(*api)||api->version!=TQT_RADAR_API_VERSION||!api->isReady||!api->setCollection||!api->copyDataDirectory||!api->registerInspector){logW("OFF: incompatible MobRadar API");return 1;}
            if(api->isReady()){
                HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<const wchar_t*>(get),&pinned);
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<const wchar_t*>(museum),&pinned);
                startLootInspector(api);startCollectionReader(api);logI("ON: MuseumRadar connected to MobRadar and Museum");return 0;
            }
        }
        Sleep(1000);
    }
}
}
void logI(const char* f,...){va_list a;va_start(a,f);logV("I",f,a);va_end(a);}
void logW(const char* f,...){va_list a;va_start(a,f);logV("W",f,a);va_end(a);}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){integration::selfModule=module;
    DisableThreadLibraryCalls(module);HANDLE t=CreateThread(nullptr,0,integration::initialize,nullptr,0,nullptr);if(t)CloseHandle(t);}return TRUE;}
