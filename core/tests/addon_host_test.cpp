#include "tqm_addon.h"
#include "game/addon_host.h"
#include "runtime.h"
#include <map>
#include <string>
#include <cstdio>
#include <cstring>
#include <cwchar>
namespace integration {
static std::map<std::string,std::string> targets={{"records/creature/alpha.dbr","Alpha"},{"records/creature/beta.dbr",u8"Бета"}};
static unsigned revision=7;
const std::map<std::string,std::string>& tooltipFarmTargets(){return targets;}
bool tooltipFarmTargetsReady(){return true;}
unsigned tooltipSourceRevision(){return revision;}
int tooltipSourceDifficulty(){return 1;}
bool safeRead(const void* from,void* to,size_t n){__try{std::memcpy(to,from,n);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void logI(const char*,...){} void logW(const char*,...){}
int32_t TQM_CALL coreReady(){return 0;}
int32_t TQM_CALL coreDataDirectory(char*,uint32_t){return 0;}
int32_t TQM_CALL coreSourceState(TqtSourceStateV1*){return 0;}
int32_t TQM_CALL coreSourceText(uint32_t,char*,uint32_t,uint32_t*){return TQM_COPY_INVALID;}
void TQM_CALL coreKeyboardBusy(uint32_t){}
int32_t TQM_CALL coreSetCollection(const TqtCollectedItemV1*,uint32_t,uint32_t){return 1;}
}
namespace {
int checks=0,failures=0,frames=0,faults=0,messages=0,keys=0;
void check(bool ok,const char* what){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",what);}}
const TqmAddonApiV1* api=nullptr;
class Recorder:public museum::ui::Renderer {
public:
    int fills=0,captions=0;museum::ui::Rect last={};
    void fill(museum::ui::Rect rect,museum::ui::Color) override{++fills;last=rect;}
    float measure(const wchar_t* text,int size) override{return float(std::wcslen(text))*size;}
    void text(museum::ui::Rect rect,const wchar_t*,int,museum::ui::Color,museum::ui::TextAlign) override{++captions;last=rect;}
};
void TQM_CALL frame(void*,const TqmFrameV1* f){
    ++frames;check(f->size==sizeof(*f)&&f->version==1&&f->difficulty==1,"frame version and context");
    uint32_t count=0;check(api->copyFarmTargets(f->sourceRevision,nullptr,0,&count)==TQM_COPY_OK&&count==2,"caller queries source count");
    TqmFarmTargetV1 copied[2];std::memset(copied,0xcc,sizeof(copied));
    check(api->copyFarmTargets(f->sourceRevision,copied,1,&count)==TQM_COPY_CAPACITY&&static_cast<unsigned char>(copied[0].record[0])==0xcc,"short buffers are untouched");
    check(api->copyFarmTargets(f->sourceRevision-1,copied,2,&count)==TQM_COPY_STALE&&static_cast<unsigned char>(copied[0].record[0])==0xcc,"stale snapshots are refused without writes");
    check(api->copyFarmTargets(f->sourceRevision,copied,2,&count)==TQM_COPY_OK&&count==2&&
          std::strcmp(copied[0].record,"records/creature/alpha.dbr")==0&&std::strcmp(copied[1].name,u8"Бета")==0,"normalized paths and UTF-8 names copied into caller memory");
    const TqmRectV1 rect={4,5,100,20};const TqmColorV1 color={1,1,1,1};
    api->fillRect(f->renderContext,&rect,&color);api->drawText(f->renderContext,&rect,L"Alpha",12,&color,0);
    check(api->measureText(f->renderContext,L"Alpha",12)==60,"native measurement forwarded inside frame");
}
void TQM_CALL fault(void*,const TqmFrameV1*){++faults;RaiseException(EXCEPTION_ACCESS_VIOLATION,0,0,nullptr);}
int32_t TQM_CALL message(void*,void*,uint32_t msg,uintptr_t,intptr_t,uint32_t busy){++messages;return !busy&&msg==WM_KEYDOWN?1:0;}
int32_t TQM_CALL key(void*,int32_t button,int32_t,uint32_t busy){++keys;return !busy&&button==0x42?1:0;}
}
int main(){
    api=TQM_GetAddonApi(1);check(api&&api->size==sizeof(*api),"versioned C API exists");
    check(!TQM_GetAddonApi(2),"incompatible API version refused");
    check(!api->registerCallbacks(nullptr),"null registration refused");
    TqmCallbacksV1 callbacks={sizeof(TqmCallbacksV1),1,nullptr,frame,message,key};
    callbacks.size=0;check(!api->registerCallbacks(&callbacks),"wrong callback layout refused");callbacks.size=sizeof(callbacks);
    uint32_t count=9;check(api->copyFarmTargets(7,nullptr,0,&count)==TQM_COPY_OUTSIDE_FRAME&&count==0,"snapshot API restricted to frame callback");
    const auto token=api->registerCallbacks(&callbacks);check(token!=0&&api->registerCallbacks(&callbacks)==token,"duplicate registrations reuse one slot");
    TqmCallbacksV1 broken={sizeof(TqmCallbacksV1),1,nullptr,fault,nullptr,nullptr};
    check(api->registerCallbacks(&broken)!=0,"second add-on can subscribe independently");
    Recorder draw;integration::addonHostFrame(&draw,1920,1080,nullptr,false);
    check(frames==1&&faults==1&&draw.fills==1&&draw.captions==1,"provider forwards frame and isolates a faulty add-on");
    integration::addonHostFrame(&draw,1920,1080,nullptr,false);
    check(frames==2&&faults==1,"faulty subscriber remains disabled while healthy subscriber runs");
    const TqmRectV1 rect={0,0,1,1};const TqmColorV1 color={1,1,1,1};api->fillRect(&draw,&rect,&color);
    check(draw.fills==2,"borrowed rendering context rejected outside callback");
    check(integration::addonHostInput(nullptr,WM_KEYDOWN,119,0,false),"addon may claim its window key");
    check(!integration::addonHostInput(nullptr,WM_KEYDOWN,119,0,true),"keyboard busy state reaches subscriber");
    int event[6]={};event[3]=0x42;event[4]=0;
    check(integration::addonHostKey(event,false),"native key press can be claimed");
    check(!integration::addonHostKey(event,true),"native keyboard busy state respected");
    event[4]=1;check(!integration::addonHostKey(event,false)&&keys==2,"release reaches engine bookkeeping without invoking subscriber");
    check(messages==2,"window callbacks dispatched exactly once");
    std::printf("Add-on host: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
