#include "game/addon_host.h"
#include "tqm_addon.h"
#include "runtime.h"
#include <cstring>
#include <algorithm>
namespace integration { namespace {
struct Slot {uint32_t token=0;TqmCallbacksV1 callbacks={};};
Slot slots[8];SRWLOCK slotLock=SRWLOCK_INIT;uint32_t nextToken=0;
thread_local const TqmFrameV1* currentFrame=nullptr;
bool copyRegistration(const TqmCallbacksV1* input,TqmCallbacksV1* output){
    return input && safeRead(input,output,sizeof(*output)) && output->size==sizeof(*output) &&
           output->version==TQM_ADDON_API_VERSION && output->onFrame;
}
uint32_t TQM_CALL registerCallbacks(const TqmCallbacksV1* input){
    TqmCallbacksV1 callbacks={};if(!copyRegistration(input,&callbacks))return 0;
    // Pin every callback's module: a plugin cannot leave dangling code pointers in the host.
    HMODULE owner=nullptr;
    const void* addresses[]={reinterpret_cast<const void*>(callbacks.onFrame),
                             reinterpret_cast<const void*>(callbacks.onWindowMessage),
                             reinterpret_cast<const void*>(callbacks.onKey)};
    for(const void* address:addresses)if(address && !GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        reinterpret_cast<const wchar_t*>(address),&owner))return 0;
    uint32_t token=0;AcquireSRWLockExclusive(&slotLock);
    for(auto& slot:slots){
        if(slot.token && slot.callbacks.onFrame==callbacks.onFrame && slot.callbacks.user==callbacks.user){token=slot.token;break;}
    }
    if(!token)for(auto& slot:slots)if(!slot.token){token=++nextToken;slot={token,callbacks};break;}
    ReleaseSRWLockExclusive(&slotLock);
    if(token)logI("add-on host: registered %u",token);return token;
}
int32_t TQM_CALL copyFarmTargets(uint32_t revision,TqmFarmTargetV1* output,uint32_t capacity,uint32_t* count){
    if(!count)return TQM_COPY_INVALID;
    *count=0;if(!currentFrame)return TQM_COPY_OUTSIDE_FRAME;
    if(revision!=currentFrame->sourceRevision || revision!=tooltipSourceRevision())return TQM_COPY_STALE;
    const auto& targets=tooltipFarmTargets();*count=uint32_t(targets.size());
    if(!output)return capacity?TQM_COPY_INVALID:TQM_COPY_OK;
    if(capacity<targets.size())return TQM_COPY_CAPACITY;
    for(const auto& row:targets)if(row.first.size()>=sizeof(output[0].record) || row.second.size()>=sizeof(output[0].name))return TQM_COPY_INVALID;
    uint32_t i=0;for(const auto& row:targets){
        std::memset(&output[i],0,sizeof(output[i]));
        std::memcpy(output[i].record,row.first.data(),row.first.size());
        std::memcpy(output[i].name,row.second.data(),row.second.size());++i;
    }
    return TQM_COPY_OK;
}
museum::ui::Renderer* renderer(void* context){
    return currentFrame && currentFrame->renderContext==context?static_cast<museum::ui::Renderer*>(context):nullptr;
}
void TQM_CALL fillRect(void* context,const TqmRectV1* rect,const TqmColorV1* color){
    auto* r=renderer(context);if(r&&rect&&color)r->fill({rect->x,rect->y,rect->w,rect->h},{color->r,color->g,color->b,color->a});
}
float TQM_CALL measureText(void* context,const wchar_t* text,int32_t size){
    auto* r=renderer(context);return r&&text?r->measure(text,size):1e6f;
}
void TQM_CALL drawText(void* context,const TqmRectV1* rect,const wchar_t* text,int32_t size,const TqmColorV1* color,int32_t alignment){
    auto* r=renderer(context);if(!r||!rect||!color||!text)return;
    const auto align=alignment==1?museum::ui::TextAlign::Center:alignment==2?museum::ui::TextAlign::Right:museum::ui::TextAlign::Left;
    r->text({rect->x,rect->y,rect->w,rect->h},text,size,{color->r,color->g,color->b,color->a},align);
}
const TqmAddonApiV1 api={sizeof(TqmAddonApiV1),TQM_ADDON_API_VERSION,registerCallbacks,copyFarmTargets,fillRect,measureText,drawText};
unsigned snapshot(Slot* output){unsigned n=0;AcquireSRWLockShared(&slotLock);
    for(const auto& slot:slots)if(slot.token)output[n++]=slot;
    ReleaseSRWLockShared(&slotLock);return n;
}
void disable(uint32_t token){AcquireSRWLockExclusive(&slotLock);
    for(auto& slot:slots)if(slot.token==token)slot={};
    ReleaseSRWLockExclusive(&slotLock);logW("add-on host: disabled %u after callback fault",token);
}
bool frameCall(const Slot* slot,const TqmFrameV1* frame){
    bool ok=true;const TqmFrameV1* previous=currentFrame;currentFrame=frame;utGuardEnter();
    __try {slot->callbacks.onFrame(slot->callbacks.user,frame);}
    __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    utGuardLeave();currentFrame=previous;return ok;
}
int windowCall(const Slot* slot,HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,bool busy){
    int result=0;bool ok=true;utGuardEnter();
    __try {result=slot->callbacks.onWindowMessage(slot->callbacks.user,hwnd,msg,wp,lp,busy?1u:0u);}
    __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    utGuardLeave();if(!ok)disable(slot->token);return ok?result:0;
}
int keyCall(const Slot* slot,int button,int state,bool busy){
    int result=0;bool ok=true;utGuardEnter();
    __try {result=slot->callbacks.onKey(slot->callbacks.user,button,state,busy?1u:0u);}
    __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    utGuardLeave();if(!ok)disable(slot->token);return ok?result:0;
}
bool readKey(const void* event,int* button,int* state){
    bool ok=false;utGuardEnter();
    __try {if(event){const auto* bytes=static_cast<const unsigned char*>(event);
        *button=*reinterpret_cast<const int*>(bytes+0x0c);*state=*reinterpret_cast<const int*>(bytes+0x10);ok=true;}}
    __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    utGuardLeave();return ok;
}
} // namespace
bool addonHostRegistered(){Slot list[8];return snapshot(list)!=0;}
void addonHostFrame(museum::ui::Renderer* draw,int width,int height,const void* player,bool busy){
    Slot list[8];const unsigned n=snapshot(list);if(!n)return;
    uint32_t flags=draw?TQM_FRAME_DRAW_AVAILABLE:0;
    if(tooltipFarmTargetsReady())flags|=TQM_FRAME_SOURCES_READY;
    if(busy)flags|=TQM_FRAME_KEYBOARD_BUSY;
    TqmFrameV1 frame={sizeof(TqmFrameV1),TQM_ADDON_API_VERSION,flags,GetTickCount(),tooltipSourceRevision(),
                      tooltipSourceDifficulty(),width,height,player,draw};
    for(unsigned i=0;i<n;++i)if(!frameCall(&list[i],&frame))disable(list[i].token);
}
bool addonHostInput(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp,bool busy){
    Slot list[8];const unsigned n=snapshot(list);
    for(unsigned i=0;i<n;++i)if(list[i].callbacks.onWindowMessage && windowCall(&list[i],hwnd,msg,wp,lp,busy))return true;
    return false;
}
bool addonHostKey(const void* event,bool busy){
    Slot list[8];const unsigned n=snapshot(list);if(!n)return false;
    int button=0,state=0;if(!readKey(event,&button,&state) || state==1)return false;
    for(unsigned i=0;i<n;++i)if(list[i].callbacks.onKey && keyCall(&list[i],button,state,busy))return true;
    return false;
}
} // namespace integration
#pragma comment(linker,"/EXPORT:TQM_GetAddonApi=_TQM_GetAddonApi")
extern "C" const TqmAddonApiV1* TQM_CALL TQM_GetAddonApi(uint32_t version){
    return version==TQM_ADDON_API_VERSION?&integration::api:nullptr;
}

#pragma comment(linker,"/EXPORT:TQT_GetCoreApi=_TQT_GetCoreApi")
extern "C" const TqtCoreApiV1* TQM_CALL TQT_GetCoreApi(uint32_t version){
    static const TqtCoreApiV1 core={sizeof(TqtCoreApiV1),TQT_CORE_API_VERSION,&integration::api,integration::coreReady,integration::coreDataDirectory,integration::coreSourceState,integration::coreSourceText,integration::coreKeyboardBusy};
    return version==TQT_CORE_API_VERSION?&core:nullptr;
}
