#include "farm_radar.h"
#include "runtime.h"
#include "backend/farm_radar_rules.h"
#include "ui/farm_radar_panel.h"
#include <cstring>
#include <cstdint>

namespace integration { namespace {
struct Vec3 {float x,y,z;};
// Engine.dll GetCoords copies 13 dwords, ret 4 (hidden output pointer).
// WorldCoords starts with WorldVec3: Region* + local Vec3, then nine basis floats.
struct WorldCoords {const void* region;Vec3 local;float basis[9];};
static_assert(sizeof(WorldCoords)==52,"AE 2.10 x86 WorldCoords ABI");
using Coords=WorldCoords*(__thiscall*)(const void*,WorldCoords*);
using Position=Vec3*(__thiscall*)(const void*,Vec3*);
using WorldIndex=int(__thiscall*)(const void*);
using Alive=bool(__thiscall*)(const void*);
Coords getCoords=nullptr;Position getWorldPosition=nullptr;WorldIndex getWorldIndex=nullptr;
Alive isAlive=nullptr;
bool resolved=false,failed=false,world=false,visible=true;
int lastConfig=-1,lastRadius=0;unsigned lastRevision=0;
DWORD lastScan=0;const void* lastPlayer=nullptr;
std::vector<museum::RadarEntry> rows;
museum::ui::FarmRadarPanel panel;
constexpr size_t aliveSlot=86;

bool bindApis(){
    if(resolved)return !failed;
    resolved=true;HMODULE e=GetModuleHandleW(L"Engine.dll"),g=GetModuleHandleW(L"Game.dll");
    getCoords=reinterpret_cast<Coords>(GetProcAddress(e,"?GetCoords@Entity@GAME@@QBE?AVWorldCoords@2@XZ"));
    getWorldPosition=reinterpret_cast<Position>(GetProcAddress(e,"?GetWorldPosition@WorldVec3@GAME@@QBE?AVVec3@2@XZ"));
    getWorldIndex=reinterpret_cast<WorldIndex>(GetProcAddress(e,"?GetWorldIndex@Region@GAME@@QBEHXZ"));
    isAlive=reinterpret_cast<Alive>(GetProcAddress(g,"?IsAlive@Character@GAME@@UBE_NXZ"));
    const auto* monster=reinterpret_cast<const void* const*>(GetProcAddress(g,"??_7Monster@GAME@@6B@"));
    const unsigned char coordsTail[]={0x8b,0x44,0x24,0x44,0x8d,0xb1,0xa0,0,0,0,0xb9,0x0d,0,0,0};
    const unsigned char positionPrefix[]={0x83,0xec,0x0c,0x8b,0x01,0xf3,0x0f,0x7e,0x40,0x2c,0x66,0x0f,0x6e,0x58,0x34};
    const unsigned char indexBody[]={0x8b,0x41,0x28,0xc3};
    unsigned char tail[sizeof(coordsTail)]={};const void* aliveEntry=nullptr;
    unsigned char positionCode[sizeof(positionPrefix)]={},indexCode[sizeof(indexBody)]={};
    // Refuse another layout before calling a structure-returning member.
    if(!getCoords || !getWorldPosition || !getWorldIndex || !isAlive || !monster ||
       !g_tq.ObjectManagerGet || !g_tq.ObjectManagerGetObjectList || !g_tq.ObjectGetObjectName ||
       !g_tq.CrtOperatorDelete || !safeRead(monster+aliveSlot,&aliveEntry,sizeof(aliveEntry)) ||
       aliveEntry!=reinterpret_cast<const void*>(isAlive) ||
       !safeRead(reinterpret_cast<const unsigned char*>(getCoords)+150,tail,sizeof(tail)) ||
       std::memcmp(tail,coordsTail,sizeof(tail))!=0 ||
       !safeRead(reinterpret_cast<const void*>(getWorldPosition),positionCode,sizeof(positionCode)) ||
       std::memcmp(positionCode,positionPrefix,sizeof(positionCode))!=0 ||
       !safeRead(reinterpret_cast<const void*>(getWorldIndex),indexCode,sizeof(indexCode)) ||
       std::memcmp(indexCode,indexBody,sizeof(indexCode))!=0){farmRadarFault("unsupported creature/coordinate ABI");return false;}
    logI("farm radar: world-coordinate ABI and Character::IsAlive slot verified; radius %d, key %d",
         g_cfg.farmRadarRadius,g_cfg.farmRadarHotkey);return true;
}
bool point(const void* actor,museum::RadarPoint* out){
    WorldCoords coords={};Vec3 p={};getCoords(actor,&coords);
    if(!coords.region)return false;
    getWorldPosition(&coords,&p);
    out->x=p.x;out->y=p.y;out->z=p.z;out->world=getWorldIndex(coords.region);
    return std::isfinite(out->x)&&std::isfinite(out->y)&&std::isfinite(out->z);
}
bool readPlayer(const void** player,museum::RadarPoint* location){
    bool ok=false;
    __try {
        *player=radarLocalPlayer();
        // A local main player with a loaded region is required, including on loading/menu edges.
        if(*player)ok=point(*player,location);
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    return ok;
}
bool listObjects(TqPtrVector* list){
    bool ok=false;
    __try {const void* manager=g_tq.ObjectManagerGet();if(manager){g_tq.ObjectManagerGetObjectList(manager,list);ok=true;}}
    __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    return ok;
}
bool freeObjects(TqPtrVector* list){
    bool ok=true;
    __try {if(list->first)g_tq.CrtOperatorDelete(const_cast<void*>(static_cast<const void*>(list->first)));}
    __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    *list={};return ok;
}
bool recordName(const void* actor,char* record){
    bool ok=false;
    __try {const char* name=g_tq.ObjectGetObjectName(actor);ok=true;record[0]=0;
        if(name)for(size_t i=0;i<512;++i){char c=name[i];
            if(c=='\\')c='/';if(c>='A'&&c<='Z')c+=32;record[i]=c;
            if(!c)break;
            if(i==511)record[0]=0;}
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    return ok;
}
bool creaturePoint(const void* actor,bool* live,museum::RadarPoint* location){
    bool ok=true;*live=false;
    __try {
        const auto* vt=*reinterpret_cast<const void* const* const*>(actor);
        // Records must ALSO be in the model's Monster-only index. Inherited IsAlive supports
        // special boss classes while rejecting controllers, loot objects, and plain entities.
        if(vt && vt[aliveSlot]==reinterpret_cast<const void*>(isAlive) && isAlive(actor))
            *live=point(actor,location);
    } __except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    return ok;
}
std::vector<museum::RadarEntry> scan(const void* player,const museum::RadarPoint& origin){
    TqPtrVector list={};std::map<std::string,double> nearest;
    bool ok=listObjects(&list);
    size_t matched=0,alive=0,inRange=0;double closest=1e30;std::string closestRecord;
    std::map<std::string,size_t> arachnids;
    const uintptr_t a=reinterpret_cast<uintptr_t>(list.first),b=reinterpret_cast<uintptr_t>(list.last),
                    c=reinterpret_cast<uintptr_t>(list.end);
    ok=ok && b>=a && c>=b && (b-a)%sizeof(void*)==0 && (b-a)/sizeof(void*)<=4000000;
    try {
        const auto& targets=tooltipFarmTargets();
        for(size_t i=0;ok && i<(b-a)/sizeof(void*);++i){
            const void* obj=nullptr;char record[512]={};
            if(!safeRead(list.first+i,&obj,sizeof(obj))){ok=false;break;}
            if(!obj || obj==player)continue;
            if(!recordName(obj,record)){ok=false;break;}
            if(std::strstr(record,"arach"))++arachnids[record];
            const auto found=targets.find(record);if(found==targets.end())continue;
            ++matched;
            bool live=false;museum::RadarPoint p;
            if(!creaturePoint(obj,&live,&p)){ok=false;break;}
            if(live){
                ++alive;const double distance=museum::radarDistanceSquared(origin,p);
                if(distance>=0&&std::isfinite(distance)&&distance<closest){closest=distance;closestRecord=record;}
                if(distance>=0&&distance<=double(g_cfg.farmRadarRadius)*g_cfg.farmRadarRadius)++inRange;
                museum::radarRemember(nearest,found->second,distance,g_cfg.farmRadarRadius);
            }
        }
    } catch(...){freeObjects(&list);throw;}
    if(!freeObjects(&list))ok=false;
    if(!ok){farmRadarFault("live object scan failed");return {};}
    static DWORD lastReport=0;const DWORD now=GetTickCount();
    if(!lastReport || now-lastReport>=10000){
        lastReport=now;
        logI("scan: objects=%zu best-record matches=%zu alive-positioned=%zu within-radius=%zu names=%zu player=(%.2f,%.2f,%.2f) world=%d",
             (b-a)/sizeof(void*),matched,alive,inRange,nearest.size(),origin.x,origin.y,origin.z,origin.world);
        if(!closestRecord.empty())logI("scan: closest best creature %.2f units: %s",std::sqrt(closest),closestRecord.c_str());
        for(const auto& entry:arachnids)logI("scan: arachnid record %s instances=%zu eligible=%d",
             entry.first.c_str(),entry.second,tooltipFarmTargets().count(entry.first)?1:0);
    }
    return museum::radarSorted(nearest);
}
} // namespace
void farmRadarFault(const char* reason){
    if(!failed)logW("farm radar: OFF for this session: %s",reason);
    failed=true;rows.clear();panel.reset();
}
void farmRadarForget(){world=false;lastPlayer=nullptr;lastScan=0;rows.clear();panel.reset();}
void farmRadarTick() try {
    if(lastConfig!=g_cfg.farmRadar){lastConfig=g_cfg.farmRadar;visible=lastConfig!=0;lastScan=0;}
    if(!g_cfg.enabled || !g_cfg.farmRadar || failed){farmRadarForget();return;}
    if(!bindApis())return;
    const void* player=nullptr;museum::RadarPoint origin;
    if(!readPlayer(&player,&origin)){farmRadarForget();return;}
    world=true;
    if(player!=lastPlayer){lastPlayer=player;lastScan=0;rows.clear();panel.reset();}
    if(!visible){rows.clear();panel.reset();return;}
    const DWORD now=GetTickCount();const unsigned revision=tooltipSourceRevision();
    if(lastScan && now-lastScan<250 && lastRevision==revision && lastRadius==g_cfg.farmRadarRadius)return;
    lastScan=now;lastRevision=revision;lastRadius=g_cfg.farmRadarRadius;
    if(tooltipFarmTargets().empty()){rows.clear();panel.reset();return;}
    rows=scan(player,origin);
} catch(...){farmRadarFault("scan exception");}
bool farmRadarShown(){return world&&visible&&!failed&&g_cfg.enabled&&g_cfg.farmRadar;}
void farmRadarDraw(museum::ui::Renderer& renderer,int width,int height){
    if(farmRadarShown())panel.draw(renderer,width,height,rows);
}
bool farmRadarInput(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(!world || failed || !g_cfg.enabled || !g_cfg.farmRadar)return false;
    if(msg==WM_KEYDOWN && g_cfg.farmRadarHotkey && int(wp)==g_cfg.farmRadarHotkey &&
       !searchFieldFocused() && !panelViewerActive()){
        if(!(lp&(1L<<30))){visible=!visible;lastScan=0;rows.clear();panel.reset();
            logI("farm radar: %s",visible?"shown":"hidden");}
        return true;
    }
    if(msg==WM_MOUSEWHEEL && farmRadarShown() && !panelViewerActive()){
        POINT p={static_cast<short>(LOWORD(lp)),static_cast<short>(HIWORD(lp))};RECT client={};
        if(!ScreenToClient(hwnd,&p) || !GetClientRect(hwnd,&client) || client.right<=0 || client.bottom<=0)return false;
        const int width=radarCanvasWidth(),height=radarCanvasHeight();
        if(width<=0 || height<=0)return false;
        const int delta=GET_WHEEL_DELTA_WPARAM(wp);
        return delta && panel.scroll(p.x*float(width)/client.right,p.y*float(height)/client.bottom,
                                     delta>0?-1:1,rows.size());
    }
    return false;
}
bool farmRadarKeyGate(int button,int state){
    if(!world || failed || !g_cfg.enabled || !g_cfg.farmRadar || !g_cfg.farmRadarHotkey ||
       searchFieldFocused() || panelViewerActive())return false;
    if(state==1)return false;
    const UINT scanCode=MapVirtualKeyW(UINT(g_cfg.farmRadarHotkey),MAPVK_VK_TO_VSC);
    // Same DirectInput scan-code convention as the existing search key gate.
    return scanCode && UINT(button)==scanCode;
}
}
