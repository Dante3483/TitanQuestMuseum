#include "runtime.h"
#include "engine_exports.h"
#include "game/addon_host.h"
#include "game/archive/generate.h"
#include "game/archive/loot_sources.h"
#include "MinHook.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cwchar>
#include <cmath>
#include <mutex>
#include <vector>
#include <algorithm>
#include <stdexcept>
#include <share.h>

namespace integration {
namespace {
HMODULE selfModule=nullptr;
FILE* logFile=nullptr;SRWLOCK logLock=SRWLOCK_INIT;
char dataDirectory[MAX_PATH]={};volatile LONG ready=0,busy=0;
void** ppEngine=nullptr;void** ppGame=nullptr;
using EngineVoid=void(__thiscall*)(void*);
using Ptr=void*(__thiscall*)(const void*);
using Int= int(__thiscall*)(const void*);
using Bool=bool(__thiscall*)(const void*);
using PlayerInfo=void(__thiscall*)(const void*,unsigned*);
using Level=unsigned(__thiscall*)(const void*);
Ptr mainPlayer=nullptr,graphics=nullptr,canvas=nullptr;
Int width=nullptr,height=nullptr;Bool custom=nullptr;
PlayerInfo playerInfo=nullptr;Level charLevel=nullptr;
EngineVoid originalPresent=nullptr;
using Key=void(__thiscall*)(void*,const void*);
Key originalKey=nullptr;
struct EngineString {union {char buffer[16];char* pointer;};unsigned size,capacity;};
static_assert(sizeof(EngineString)==24,"VS2012 x86 string ABI");
using Font=const void*(__thiscall*)(void*,const EngineString*,unsigned,unsigned);
using Fill=void(__thiscall*)(void*,const TqmRectV1*,const TqmColorV1*,const void*,const void*);
using Measure=float(__thiscall*)(void*,int,int,const TqmColorV1*,const wchar_t*,const void*,int,int,int,unsigned,int,int,int);
using Text=float(__thiscall*)(void*,TqmRectV1,const TqmColorV1*,const wchar_t*,const void*,int,int,int,unsigned,int,int,unsigned,int,unsigned,unsigned);
Font loadFont=nullptr;Fill fill=nullptr;Measure measure=nullptr;Text text=nullptr;
const void* font=nullptr;bool fontTried=false;
std::mutex stateMutex;
HANDLE sourceEvent=nullptr;
gen::LootContext wanted,activeContext,resultContext;
bool pending=false,contextValid=false,resultReady=false,failed=false;
std::string sourceText;
std::map<std::string,std::string> resultTargets,frameTargets;
unsigned revision=0,frameRevision=0;int frameDifficulty=-1;bool frameReady=false;
DWORD lastContextTick=0;
HWND gameWindow=nullptr;WNDPROC oldWindowProc=nullptr;bool unicodeWindow=false;

void logV(const char* level,const char* format,va_list args){
    AcquireSRWLockExclusive(&logLock);
    if(logFile){std::fprintf(logFile,"%s ",level);std::vfprintf(logFile,format,args);std::fputc('\n',logFile);std::fflush(logFile);}
    ReleaseSRWLockExclusive(&logLock);
}
template<class T> bool bind(T& out,HMODULE module,const char* name){
    out=reinterpret_cast<T>(GetProcAddress(module,name));
    if(!out)logW("missing required export %s",name);return out!=nullptr;
}
void* pointer(void** address){void* value=nullptr;if(address)safeRead(address,&value,sizeof(value));return value;}
bool readContext(gen::LootContext* out,const void** player){
    unsigned info[5]={};bool ok=false;*player=nullptr;
    __try {
        void* e=pointer(ppEngine);void* g=pointer(ppGame);
        if(e&&g&&!custom(e)){
            *player=mainPlayer(g);
            if(*player){const unsigned level=charLevel(*player);
                if(level>=1&&level<=120){playerInfo(g,info);ok=true;}}
        }
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    if(!ok){*player=nullptr;return false;}
    *out={int(info[0]),int(info[1]),int(info[2]),int(info[3]),int(info[4])};
    if(!out->valid()){*player=nullptr;return false;}return true;
}
DWORD WINAPI calculateSources(void*){
    try {
        gen::LootSourceModel model;std::string error;
        if(!model.load(std::string(dataDirectory)+"\\loot-model.bin",&error)){
            std::lock_guard<std::mutex> lock(stateMutex);failed=true;logW("source model: %s",error.c_str());return 1;
        }
        struct Cache {gen::LootContext context;std::string text;std::map<std::string,std::string> targets;};
        std::vector<Cache> cache;
        for(;;){
            if(WaitForSingleObject(sourceEvent,INFINITE)!=WAIT_OBJECT_0)return 1;
            gen::LootContext context;
            {std::lock_guard<std::mutex> lock(stateMutex);if(!pending)continue;context=wanted;pending=false;}
            std::string output;std::map<std::string,std::string> targets;
            auto found=std::find_if(cache.begin(),cache.end(),[&](const auto& c){return c.context==context;});
            if(found!=cache.end()){output=found->text;targets=found->targets;}
            else {
                if(!model.calculate(context,output,&error,&targets))throw std::runtime_error(error);
                if(cache.size()>=4)cache.erase(cache.begin());cache.push_back({context,output,targets});
            }
            {std::lock_guard<std::mutex> lock(stateMutex);
                if(contextValid&&context==activeContext){sourceText=std::move(output);resultTargets=std::move(targets);
                    resultContext=context;resultReady=true;++revision;}}
            logI("sources calculated: level %d, party %d, difficulty %d",context.averageLevel,context.players,context.difficulty);
        }
    } catch(...){std::lock_guard<std::mutex> lock(stateMutex);failed=true;logW("source worker failed");}
    return 1;
}
void tickSources(const gen::LootContext& context,bool valid){
    std::lock_guard<std::mutex> lock(stateMutex);
    if(!valid){if(contextValid||resultReady){contextValid=false;resultReady=false;sourceText.clear();resultTargets.clear();++revision;}}
    else if(!contextValid||!(context==activeContext)){
        activeContext=context;contextValid=true;resultReady=false;sourceText.clear();resultTargets.clear();++revision;
        wanted=context;pending=true;SetEvent(sourceEvent);
    }
    if(frameRevision!=revision)frameTargets=resultTargets;
    frameReady=contextValid&&resultReady&&!failed;
    frameRevision=revision;frameDifficulty=contextValid?activeContext.difficulty:-1;
}
bool verifyTextAlignment(const unsigned char* wrapper){
    unsigned char code[256];if(!safeRead(wrapper,code,sizeof(code)))return false;
    for(int i=0;i<180;++i){if(code[i]!=0xe8)continue;int displacement=0;std::memcpy(&displacement,code+i+1,4);
        unsigned char body[0xa0];if(!safeRead(wrapper+i+5+displacement,body,sizeof(body)))continue;
        const unsigned char first[]={0x8b,0x44,0x24,0x20,0xf3,0x0f,0x10,0x44,0x24,0x08};
        if(!std::memcmp(body,first,sizeof(first))&&body[0x17]==0x48&&body[0x18]==0x74&&body[0x1a]==0x48&&body[0x1b]==0x75&&
           body[0x63]==0x48&&body[0x64]==0x74&&body[0x66]==0x48&&body[0x67]==0x75)return true;
    }return false;
}
class Renderer final:public museum::ui::Renderer {
    void* canvas_;
public:
    explicit Renderer(void* c):canvas_(c){}
    void fill(museum::ui::Rect r,museum::ui::Color color) override {
        const TqmRectV1 rect={r.x,r.y,r.w,r.h};const TqmColorV1 c={color.r,color.g,color.b,color.a};
        integration::fill(canvas_,&rect,&c,nullptr,nullptr);
    }
    float measure(const wchar_t* value,int size) override {
        const TqmColorV1 transparent={1,1,1,0};
        const float result=integration::measure(canvas_,-30000,-30000,&transparent,value,font,size,0,0,0,0,0,0);
        return std::isfinite(result)&&result>=0?result:1e6f;
    }
    void text(museum::ui::Rect r,const wchar_t* value,int size,museum::ui::Color color,museum::ui::TextAlign alignment) override {
        const TqmRectV1 rect={r.x,r.y,r.w,r.h};const TqmColorV1 c={color.r,color.g,color.b,color.a};
        const int x=alignment==museum::ui::TextAlign::Center?2:alignment==museum::ui::TextAlign::Right?1:0;
        integration::text(canvas_,rect,&c,value,font,size,x,2,0,0,0,0,0,0,0);
    }
};
LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(addonHostInput(hwnd,msg,wp,lp,InterlockedCompareExchange(&busy,0,0)!=0))return 0;
    return unicodeWindow?CallWindowProcW(oldWindowProc,hwnd,msg,wp,lp):CallWindowProcA(oldWindowProc,hwnd,msg,wp,lp);
}
BOOL CALLBACK findWindow(HWND hwnd,LPARAM){
    DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
    if(pid==GetCurrentProcessId()&&GetWindow(hwnd,GW_OWNER)==nullptr&&IsWindowVisible(hwnd)){gameWindow=hwnd;return FALSE;}return TRUE;
}
void attachInput(){
    if(oldWindowProc)return;gameWindow=nullptr;EnumWindows(findWindow,0);if(!gameWindow)return;
    unicodeWindow=IsWindowUnicode(gameWindow)!=FALSE;SetLastError(0);
    const LONG previous=unicodeWindow?SetWindowLongW(gameWindow,GWL_WNDPROC,reinterpret_cast<LONG>(windowProc)):
        SetWindowLongA(gameWindow,GWL_WNDPROC,reinterpret_cast<LONG>(windowProc));
    if(previous)oldWindowProc=reinterpret_cast<WNDPROC>(previous);
}
void frame(){
    gen::LootContext context;const void* player=nullptr;const bool valid=readContext(&context,&player);
    const DWORD now=GetTickCount();
    if(!lastContextTick||now-lastContextTick>=500||!valid){lastContextTick=now;tickSources(context,valid);}
    else {std::lock_guard<std::mutex> lock(stateMutex);if(frameRevision!=revision)frameTargets=resultTargets;frameRevision=revision;
        frameReady=contextValid&&resultReady&&!failed;frameDifficulty=contextValid?activeContext.difficulty:-1;}
    attachInput();
    void* e=pointer(ppEngine);void* gfx=e?graphics(e):nullptr;void* c=gfx?canvas(gfx):nullptr;
    if(!c){addonHostFrame(nullptr,0,0,player,busy!=0);return;}
    if(player&&!fontTried){fontTried=true;static char name[]="fonts/albertus mt light.fnt";
        EngineString str={};str.pointer=name;str.size=unsigned(std::strlen(name));str.capacity=str.size;
        font=loadFont(gfx,&str,1,0);logI("native font %s",font?"loaded":"unavailable");}
    if(!player){font=nullptr;fontTried=false;}
    if(!font){addonHostFrame(nullptr,width(c),height(c),player,busy!=0);return;}
    Renderer renderer(c);addonHostFrame(&renderer,width(c),height(c),player,busy!=0);
}
void guardedFrame(){__try {frame();}__except(EXCEPTION_EXECUTE_HANDLER){logW("guarded frame fault");}}
void __fastcall present(void* self,void*){
    try {guardedFrame();}catch(...){logW("frame exception");}
    originalPresent(self);
}
void __fastcall key(void* self,void*,const void* event){
    if(addonHostKey(event,InterlockedCompareExchange(&busy,0,0)!=0))return;
    originalKey(self,event);
}
bool bindGame(){
    HMODULE e=GetModuleHandleW(L"Engine.dll"),g=GetModuleHandleW(L"Game.dll");bool ok=true;
#define B(var,module,name) ok=bind(var,module,name)&&ok
    B(ppEngine,e,TQ_ENGINE_GENGINE);B(ppGame,g,TQ_GAMEENGINE_GGAMEENGINE);
    B(mainPlayer,g,TQ_GAMEENGINE_GETMAINPLAYER);B(custom,e,TQ_ENGINE_HASLOADEDCUSTOMDATABASE);
    B(graphics,e,TQ_ENGINE_GETGRAPHICSENGINE);B(canvas,e,TQ_GFX_GETCANVAS);
    B(width,e,TQ_GFX_CANVAS_GETWIDTH);B(height,e,TQ_GFX_CANVAS_GETHEIGHT);
    B(loadFont,e,TQ_GFX_LOADFONT);B(fill,e,TQ_GFX_CANVAS_RENDERRECT);B(measure,e,TQ_GFX_CANVAS_RENDERTEXT_W);
    B(playerInfo,g,"?GetPlayerInfo@GameEngine@GAME@@QBEXAAUPlayerInfo@2@@Z");B(charLevel,g,"?GetCharLevel@Character@GAME@@QBE?BIXZ");
    B(text,e,"?RenderText@GraphicsCanvas@GAME@@QAEMVRect@2@ABVColor@2@PBGPBVGraphicsFont@2@HW4XAlignment@12@W4YAlignment@12@_NHW4RenderFontStyle@2@66H6@Z");
#undef B
    return ok&&verifyTextAlignment(reinterpret_cast<const unsigned char*>(text));
}
DWORD WINAPI initialize(void*){
    try {
        wchar_t path[MAX_PATH]={};if(!GetModuleFileNameW(selfModule,path,MAX_PATH))return 1;
        wchar_t* slash=std::wcsrchr(path,L'\\');if(!slash)return 1;*slash=0;
        wchar_t folder[MAX_PATH]={},ini[MAX_PATH]={},logPath[MAX_PATH]={};
        swprintf_s(folder,L"%s\\TitanQuestCore",path);CreateDirectoryW(folder,nullptr);
        swprintf_s(ini,L"%s\\TitanQuestCore.ini",folder);swprintf_s(logPath,L"%s\\TitanQuestCore.log",folder);
        logFile=_wfsopen(logPath,L"w",_SH_DENYNO);logI("Titan Quest Toolkit Core 0.1 x86");
        const DWORD modulesStart=GetTickCount();
        while(!GetModuleHandleW(L"Engine.dll")||!GetModuleHandleW(L"Game.dll")){
            if(GetTickCount()-modulesStart>=60000){logW("game modules did not load");return 1;}Sleep(200);
        }
        if(!WideCharToMultiByte(CP_ACP,WC_NO_BEST_FIT_CHARS,folder,-1,dataDirectory,MAX_PATH,nullptr,nullptr))return 1;
        wchar_t language[16]={};GetPrivateProfileStringW(L"core",L"text_language",L"",language,16,ini);
        if(!language[0]){
            wchar_t legacy[MAX_PATH]={};swprintf_s(legacy,L"%s\\TitanQuestMuseum\\TitanQuestMuseum.ini",path);
            GetPrivateProfileStringW(L"advanced",L"text_language",L"EN",language,16,legacy);
            for(unsigned i=0;language[i];++i)if(language[i]==L' '||language[i]==L';'){language[i]=0;break;}
            WritePrivateProfileStringW(L"core",L"text_language",language,ini);
        }
        char lang[16]={};unsigned n=0;for(;language[n]&&n<7;++n){wchar_t ch=language[n];
            if(ch>=L'a'&&ch<=L'z')ch-=32;if(!((ch>=L'A'&&ch<=L'Z')||(ch>=L'0'&&ch<=L'9')))break;lang[n]=char(ch);}
        if(!n||language[n])strcpy_s(lang,"EN");
        wchar_t exe[MAX_PATH]={};GetModuleFileNameW(nullptr,exe,MAX_PATH);slash=std::wcsrchr(exe,L'\\');if(!slash)return 1;*slash=0;
        char gameDir[MAX_PATH]={};WideCharToMultiByte(CP_ACP,0,exe,-1,gameDir,MAX_PATH,nullptr,nullptr);
        logI("shared data: %s, Text_%s",dataDirectory,lang);
        gen::GenReport report;if(!gen::ensureOutputsGuarded(gameDir,dataDirectory,lang,report)){logW("generation failed: %s",report.error.c_str());return 1;}
        logI("shared data ready: %s",report.summary.c_str());
        if(!bindGame()){logW("required game bindings unavailable");return 1;}
        sourceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!sourceEvent)return 1;
        HANDLE worker=CreateThread(nullptr,0,calculateSources,nullptr,0,nullptr);if(!worker)return 1;CloseHandle(worker);
        auto target=GetProcAddress(GetModuleHandleW(L"Engine.dll"),TQ_ENGINE_PRESENTSURFACE);
        auto keyTarget=GetProcAddress(GetModuleHandleW(L"Engine.dll"),TQ_INPUT_DISPLAY_HANDLEKEYEVENT);
        if(!target||!keyTarget||MH_Initialize()!=MH_OK)return 1;
        if(MH_CreateHook(target,reinterpret_cast<void*>(present),reinterpret_cast<void**>(&originalPresent))!=MH_OK||
           MH_CreateHook(keyTarget,reinterpret_cast<void*>(key),reinterpret_cast<void**>(&originalKey))!=MH_OK){logW("hook creation failed");return 1;}
        if(MH_EnableHook(MH_ALL_HOOKS)!=MH_OK){logW("hook activation failed");return 1;}
        InterlockedExchange(&ready,1);logI("READY: standalone data, source service, frame and input host; Museum optional");
    }catch(...){logW("initialization exception");return 1;}return 0;
}
} // namespace
bool safeRead(const void* from,void* to,size_t size){__try {std::memcpy(to,from,size);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}}
void logI(const char* f,...){va_list a;va_start(a,f);logV("I",f,a);va_end(a);}
void logW(const char* f,...){va_list a;va_start(a,f);logV("W",f,a);va_end(a);}
const std::map<std::string,std::string>& tooltipFarmTargets(){return frameTargets;}
bool tooltipFarmTargetsReady(){return frameReady;}
unsigned tooltipSourceRevision(){return frameRevision;}
int tooltipSourceDifficulty(){return frameDifficulty;}
int32_t TQM_CALL coreReady(){return InterlockedCompareExchange(&ready,0,0)!=0;}
int32_t TQM_CALL coreDataDirectory(char* out,uint32_t capacity){
    if(!coreReady()||!out||capacity<=std::strlen(dataDirectory))return 0;strcpy_s(out,capacity,dataDirectory);return 1;
}
int32_t TQM_CALL coreSourceState(TqtSourceStateV1* out){
    if(!out||out->size!=sizeof(*out))return 0;std::lock_guard<std::mutex> lock(stateMutex);
    *out={sizeof(*out),revision,uint32_t(contextValid&&resultReady&&!failed),activeContext.averageLevel,
        activeContext.minLevel,activeContext.maxLevel,activeContext.players,contextValid?activeContext.difficulty:-1};return 1;
}
int32_t TQM_CALL coreSourceText(uint32_t wantedRevision,char* out,uint32_t capacity,uint32_t* bytes){
    if(!bytes)return TQM_COPY_INVALID;*bytes=0;std::lock_guard<std::mutex> lock(stateMutex);
    if(wantedRevision!=revision)return TQM_COPY_STALE;
    if(!contextValid||!resultReady||failed)return TQM_COPY_INVALID;
    *bytes=uint32_t(sourceText.size()+1);if(!out)return capacity?TQM_COPY_INVALID:TQM_COPY_OK;
    if(capacity<*bytes)return TQM_COPY_CAPACITY;std::memcpy(out,sourceText.c_str(),*bytes);return TQM_COPY_OK;
}
void TQM_CALL coreKeyboardBusy(uint32_t value){InterlockedExchange(&busy,value?1:0);}
#ifdef TQT_CORE_TEST
void coreTestPublish(const gen::LootContext& context,const std::string& value,const std::map<std::string,std::string>& targets){
    std::lock_guard<std::mutex> lock(stateMutex);
    activeContext=context;contextValid=context.valid();resultReady=contextValid;
    sourceText=contextValid?value:std::string();resultTargets=contextValid?targets:std::map<std::string,std::string>();
    frameTargets=resultTargets;frameReady=contextValid;frameDifficulty=contextValid?context.difficulty:-1;
    ++revision;frameRevision=revision;strcpy_s(dataDirectory,"fixture-core");InterlockedExchange(&ready,1);
}
#endif
} // namespace integration
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){integration::selfModule=module;DisableThreadLibraryCalls(module);
        HANDLE thread=CreateThread(nullptr,0,integration::initialize,nullptr,0,nullptr);if(thread)CloseHandle(thread);}
    return TRUE;
}
