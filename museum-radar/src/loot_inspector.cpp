#include "loot_inspector.h"
#include "runtime.h"
#include "backend/loot_identity.h"
#include "backend/model/catalogue.h"
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <map>
#include <vector>
#include <algorithm>

namespace integration {namespace {
gdut::Catalogue catalogue;
std::set<std::string> collected;bool collectionKnown=false;
std::mutex collectionMutex;
struct IdVector {const unsigned* first;const unsigned* last;const unsigned* end;};
struct GameWideString {union {wchar_t buffer[8];wchar_t* pointer;};uint32_t size,capacity;};
static_assert(sizeof(GameWideString)==24,"VS2012 x86 wide string ABI");
using Id=unsigned(__thiscall*)(const void*);
using Inventory=const IdVector*(__thiscall*)(const void*);
using Equipment=const void*(__thiscall*)(const void*);
using Record=const char*(__thiscall*)(const void*);
using Description=GameWideString*(__thiscall*)(const void*,GameWideString*,bool,bool);
using Delete=void(__cdecl*)(void*);
Id objectId=nullptr,controllerId=nullptr,equipItems[10]={};
Inventory inventory=nullptr;Equipment equipment=nullptr;
Record objectRecord=nullptr;Description description=nullptr;Delete engineDelete=nullptr;
bool codeMatches(const void* proc,uint32_t offset,const unsigned char* expected,uint32_t bytes){
    __try {return std::memcmp(static_cast<const unsigned char*>(proc)+offset,expected,bytes)==0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}

bool readId(const void* object,unsigned* id){
    __try {*id=objectId(object);return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool inventoryIds(const void* actor,IdVector* ids,unsigned* controller){
    __try {
        const auto* source=inventory(actor);if(!source)return false;*ids=*source;*controller=controllerId(actor);
        const uintptr_t a=reinterpret_cast<uintptr_t>(ids->first),b=reinterpret_cast<uintptr_t>(ids->last),c=reinterpret_cast<uintptr_t>(ids->end);
        return b>=a&&c>=b&&(b-a)%sizeof(unsigned)==0&&(b-a)/sizeof(unsigned)<=512;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool copyIds(const IdVector& ids,unsigned* output,uint32_t* count){
    __try {*count=uint32_t((reinterpret_cast<uintptr_t>(ids.last)-reinterpret_cast<uintptr_t>(ids.first))/sizeof(unsigned));
        if(*count)std::memcpy(output,ids.first,*count*sizeof(unsigned));return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool equipmentIds(const void* controller,unsigned* output){
    __try {const auto* gear=equipment(controller);if(!gear)return false;
        for(unsigned i=0;i<10;++i)output[i]=equipItems[i](gear);return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool recordOf(const void* item,char* output){
    __try {const char* source=objectRecord(item);if(!source)return false;
        for(unsigned i=0;i<512;++i){char ch=source[i];if(ch=='\\')ch='/';if(ch>='A'&&ch<='Z')ch+=32;
            output[i]=ch;if(!ch)return i>0;}return false;
    }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
bool monsterName(const void* actor,char* output){
    GameWideString name={};bool ok=false;
    __try {
        description(actor,&name,false,false);
        if(name.size>0&&name.size<4096&&name.capacity>=name.size){
            const wchar_t* text=name.capacity<8?name.buffer:name.pointer;
            if(text){const int n=WideCharToMultiByte(CP_UTF8,0,text,int(name.size),output,1023,nullptr,nullptr);
                if(n>0){output[n]=0;ok=true;}}
        }
        if(name.capacity>=8&&name.capacity<65536&&name.pointer)engineDelete(name.pointer);
    }__except(EXCEPTION_EXECUTE_HANDLER){ok=false;}
    return ok;
}
bool objectAt(const void* const* objects,uint32_t index,const void** output){
    __try {*output=objects[index];return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
void resolve(const TqtRadarScanV1& scan,std::map<unsigned,const void*>& wanted){
    size_t remaining=0;for(const auto& pair:wanted)if(!pair.second)++remaining;
    for(uint32_t i=0;i<scan.objectCount&&remaining;++i){const void* object=nullptr;unsigned id=0;
        if(!objectAt(scan.objects,i,&object)||!object||!readId(object,&id))continue;
        const auto found=wanted.find(id);
        if(found!=wanted.end()&&!found->second){found->second=object;--remaining;}
    }
}
uint32_t TQM_CALL inspect(void*,const TqtRadarScanV1* snapshot,TqtRadarLootV1* output,uint32_t capacity){
    if(!snapshot||snapshot->size!=sizeof(*snapshot)||snapshot->mobCount>256||snapshot->objectCount>4000000||!output)return 0;
    struct Mob {uint32_t index;unsigned controller;std::set<unsigned> items;};
    std::vector<Mob> mobs;std::map<unsigned,const void*> wanted;
    for(uint32_t i=0;i<snapshot->mobCount;++i){IdVector ids={};unsigned controller=0,items[512]={};uint32_t count=0;
        if(!inventoryIds(snapshot->mobs[i].actor,&ids,&controller)||!copyIds(ids,items,&count))continue;
        Mob mob={i,controller,{}};
        for(uint32_t j=0;j<count;++j)if(items[j]){mob.items.insert(items[j]);wanted.emplace(items[j],nullptr);}
        if(controller)wanted.emplace(controller,nullptr);mobs.push_back(std::move(mob));
    }
    if(wanted.empty())return 0;
    resolve(*snapshot,wanted);
    for(auto& mob:mobs){const auto controller=wanted.find(mob.controller);unsigned gear[10]={};
        if(controller!=wanted.end()&&controller->second&&equipmentIds(controller->second,gear))
            for(unsigned id:gear)if(id){mob.items.insert(id);wanted.emplace(id,nullptr);}
    }
    resolve(*snapshot,wanted);
    std::lock_guard<std::mutex> lock(collectionMutex);uint32_t count=0;
    for(const auto& mob:mobs){std::set<std::string> records;char name[1024]={};bool named=false;
        for(unsigned id:mob.items){const auto object=wanted.find(id);char record[512]={};
            if(object==wanted.end()||!object->second||!recordOf(object->second,record))continue;
            const int item=catalogue.indexOfRecord(record);
            if(item<0||!records.insert(record).second)continue;
            if(!named){named=monsterName(snapshot->mobs[mob.index].actor,name);
                if(!named){static bool warned=false;if(!warned){warned=true;logW("inventory highlighting: monster name unavailable; entry skipped");}break;}}
            if(count>=capacity)return count;
            const auto caption=radar::lootCaption(record,std::string(catalogue.item(size_t(item)).name),collected,collectionKnown);
            auto& row=output[count++];row={};row.mobIndex=mob.index;
            std::memcpy(row.mobName,name,std::strlen(name)+1);
            const size_t bytes=(std::min)(caption.size(),sizeof(row.itemName)-1);std::memcpy(row.itemName,caption.data(),bytes);row.itemName[bytes]=0;
        }
    }
    return count;
}
template<class T>bool bind(T& target,HMODULE module,const char* name){target=reinterpret_cast<T>(GetProcAddress(module,name));return target!=nullptr;}
}
void setLootCollection(const std::set<std::string>& records,bool known){std::lock_guard<std::mutex> lock(collectionMutex);collected=records;collectionKnown=known;}
void startLootInspector(const TqtMobRadarApiV2* api){
    char folder[MAX_PATH]={};std::string error;
    if(!api->copyDataDirectory(folder,MAX_PATH)||!catalogue.loadFromFile(std::string(folder)+"\\catalogue.bin",&error)){
        logW("inventory highlighting unavailable: catalogue %s",error.c_str());return;
    }
    HMODULE game=GetModuleHandleW(L"Game.dll"),engine=GetModuleHandleW(L"Engine.dll"),crt=GetModuleHandleW(L"MSVCR110.dll");
    bool ok=bind(objectId,engine,"?GetObjectId@Object@GAME@@QBEIXZ")&&bind(objectRecord,engine,"?GetObjectName@Object@GAME@@QBEPBDXZ")&&
        bind(inventory,game,"?GetInventoryItems@Character@GAME@@QBEABV?$vector@IV?$allocator@I@std@@@std@@XZ")&&
        bind(controllerId,game,"?GetControllerId@Character@GAME@@QBE?BIXZ")&&
        bind(equipment,game,"?GetEquipmentCtrl@ControllerCharacter@GAME@@QAEAAVEquipmentCtrl@2@XZ")&&
        bind(description,game,"?GetGameDescription@Monster@GAME@@UBE?AV?$basic_string@GU?$char_traits@G@std@@V?$allocator@G@2@@std@@_N0@Z")&&
        bind(engineDelete,crt,"??3@YAXPAX@Z");
    const char* slots[]={"Head","UpperBody","Forearm","LowerBody","HandLeft","HandRight","Finger1","Finger2","Neck","Artifact"};
    for(unsigned i=0;i<10;++i){const std::string name=std::string("?GetItem_")+slots[i]+"@EquipmentCtrl@GAME@@QBEIXZ";ok=bind(equipItems[i],game,name.c_str())&&ok;}
    if(!ok){logW("inventory highlighting unavailable: required game export missing");return;}
    const unsigned char invBody[]={0x8d,0x81,0xa4,0x07,0,0,0xc3};
    const unsigned char controllerBody[]={0x8b,0x81,0x10,0x0c,0,0,0xc3};
    const unsigned char equipBody[]={0x8d,0x81,0xf0,0,0,0,0xc3};
    const unsigned char descriptionReturn[]={0xc2,0x0c,0};
    if(!codeMatches(reinterpret_cast<const void*>(inventory),0,invBody,sizeof(invBody))||
       !codeMatches(reinterpret_cast<const void*>(controllerId),0,controllerBody,sizeof(controllerBody))||
       !codeMatches(reinterpret_cast<const void*>(equipment),0,equipBody,sizeof(equipBody))||
       !codeMatches(reinterpret_cast<const void*>(description),370,descriptionReturn,sizeof(descriptionReturn))){
        logW("inventory highlighting unavailable: unsupported inventory/name ABI");return;
    }
    const TqtRadarInspectorV1 callbacks={sizeof(TqtRadarInspectorV1),nullptr,inspect};
    if(!api->registerInspector(&callbacks)){logW("inventory highlighting unavailable: MobRadar rejected inspector");return;}
    logI("inventory highlighting ready: real inventory/equipment, known names only, one scan per second");
}
}
