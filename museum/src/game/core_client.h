#pragma once
#include "tqt_core.h"
#include <windows.h>
#include <cstring>
#include <cstdio>
namespace integration {
inline const TqtCoreApiV1* coreApi(){
    HMODULE module=GetModuleHandleW(L"TitanQuestCore.asi");if(!module)return nullptr;
    auto get=reinterpret_cast<TqtGetCoreApi>(GetProcAddress(module,"TQT_GetCoreApi"));
    const auto* api=get?get(TQT_CORE_API_VERSION):nullptr;
    return api&&api->size==sizeof(*api)&&api->version==TQT_CORE_API_VERSION?api:nullptr;
}
inline bool coreConnect(DWORD timeout){
    const DWORD start=GetTickCount();do {
        const auto* api=coreApi();if(api&&api->isReady()){
            HMODULE pinned=nullptr;return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                reinterpret_cast<const wchar_t*>(api->isReady),&pinned)!=FALSE;
        }Sleep(200);
    }while(GetTickCount()-start<timeout);return false;
}
inline bool coreSharedFile(const char* leaf,char* out,size_t capacity){
    if(!leaf||!out||!capacity)return false;
    const bool shared=!std::strcmp(leaf,"catalogue.bin")||!std::strcmp(leaf,"catalogue.stamp")||
        !std::strcmp(leaf,"uniq-records.txt")||!std::strcmp(leaf,"uniq-groups.txt")||
        !std::strcmp(leaf,"uniq-excluded.txt")||!std::strcmp(leaf,"loot-model.bin")||
        !std::strcmp(leaf,"loot-sources.txt")||!std::strcmp(leaf,"gray")||!std::strncmp(leaf,"gray/",5)||!std::strncmp(leaf,"gray\\",5);
    if(!shared)return false;const auto* api=coreApi();char dir[MAX_PATH]={};
    if(!api||!api->copyDataDirectory(dir,MAX_PATH))return false;
    return _snprintf_s(out,capacity,_TRUNCATE,"%s\\%s",dir,leaf)>=0;
}
}
