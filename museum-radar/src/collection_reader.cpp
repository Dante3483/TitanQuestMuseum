#include "collection_reader.h"
#include "backend/journal_collection.h"
#include "backend/file_stamp.h"
#include "runtime.h"
#include "loot_inspector.h"
#include <cwchar>
#include <cstring>
namespace integration {namespace {
const TqtMobRadarApiV2* provider=nullptr;
radar::JournalCache cache;std::wstring cachedPath;
radar::FileStamp stamp(const BY_HANDLE_FILE_INFORMATION& info){
    return {(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow,
        (uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime,
        (uint64_t(info.nFileIndexHigh)<<32)|info.nFileIndexLow,info.dwVolumeSerialNumber};
}
bool read(const wchar_t* path,std::string& text,bool& changed){
    changed=false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){cache.forget();return false;}
    BY_HANDLE_FILE_INFORMATION info={};if(!GetFileInformationByHandle(file,&info)){CloseHandle(file);cache.forget();return false;}
    const auto current=stamp(info);
    if(cachedPath!=path){cache.forget();cachedPath=path;}
    if(!cache.needsRead(current)){CloseHandle(file);return true;}
    bool ok=current.size>0&&current.size<=64*1024*1024;
    if(ok){text.resize(size_t(current.size));DWORD count=0;ok=ReadFile(file,&text[0],DWORD(text.size()),&count,nullptr)&&count==text.size();}
    BY_HANDLE_FILE_INFORMATION after={};ok=ok&&GetFileInformationByHandle(file,&after)&&stamp(after)==current;
    CloseHandle(file);if(ok){cache.accept(current);changed=true;}else cache.forget();return ok;
}
bool journalPath(HMODULE museum,wchar_t* output){
    wchar_t folder[MAX_PATH]={};DWORD n=GetEnvironmentVariableW(L"TITANQUESTMUSEUM_OUT",folder,MAX_PATH);
    if(n>0&&n<MAX_PATH){swprintf_s(output,MAX_PATH,L"%s\\tq-uniq-items.jsonl",folder);return true;}
    wchar_t module[MAX_PATH]={};if(!GetModuleFileNameW(museum,module,MAX_PATH))return false;
    wchar_t* slash=std::wcsrchr(module,L'\\');if(!slash)return false;*slash=0;
    swprintf_s(output,MAX_PATH,L"%s\\TitanQuestMuseum\\tq-uniq-items.jsonl",module);
    if(GetFileAttributesW(output)!=INVALID_FILE_ATTRIBUTES)return true;
    wchar_t profile[MAX_PATH]={};n=GetEnvironmentVariableW(L"USERPROFILE",profile,MAX_PATH);
    if(n>0&&n<MAX_PATH)for(const wchar_t* docs:{L"Documents",L"OneDrive\\Documents"}){
        swprintf_s(output,MAX_PATH,L"%s\\%s\\My Games\\Titan Quest - Immortal Throne\\TitanQuestMuseum\\tq-uniq-items.jsonl",profile,docs);
        if(GetFileAttributesW(output)!=INVALID_FILE_ATTRIBUTES)return true;
    }
    return false;
}
DWORD WINAPI watcher(void*){
    std::set<std::string> previous,cachedKeys;bool previousKnown=false,cachedKnown=false;bool reported=false,faultLogged=false;
    for(;;){
        try {
            HMODULE museum=GetModuleHandleW(L"TitanQuestMuseum.asi");wchar_t path[MAX_PATH]={};std::string text;
            bool changed=false;const bool available=museum&&journalPath(museum,path)&&read(path,text,changed);
            if(available&&changed)cachedKnown=radar::collectedJournal(text,cachedKeys);
            if(!available){cache.forget();cachedKnown=false;cachedKeys.clear();}
            const bool known=available&&cachedKnown;std::set<std::string> collected=known?cachedKeys:std::set<std::string>();
            if(!reported||known!=previousKnown||collected!=previous){
                std::vector<TqtCollectedItemV1> copied(collected.size());size_t i=0;bool valid=true;
                for(const auto& record:collected){if(record.size()>=sizeof(copied[i].record)){valid=false;break;}
                    std::memcpy(copied[i++].record,record.c_str(),record.size()+1);}
                if(!valid){copied.clear();collected.clear();}
                const bool active=known&&valid;
                setLootCollection(collected,active);
                if(provider->setCollection(valid&&known?copied.data():nullptr,valid&&known?unsigned(copied.size()):0,valid&&known?1:0)){
                    if(!reported||active!=previousKnown){
                        if(!active&&previousKnown)logW("Museum collection unavailable or invalid; all best sources restored");
                        else logI("Museum filter: %s, %zu collected records",active?"active":"unavailable",collected.size());
                    }
                    previousKnown=active;previous=collected;reported=true;
                }
            }
        }catch(...){
            if(!faultLogged){faultLogged=true;logW("Museum collection watcher exception; filter reset (further exceptions suppressed)");}
            setLootCollection({},false);provider->setCollection(nullptr,0,0);previousKnown=false;previous.clear();reported=false;
        }
        Sleep(1000);
    }
}
}
void startCollectionReader(const TqtMobRadarApiV2* api){
    provider=api;HANDLE thread=CreateThread(nullptr,0,watcher,nullptr,0,nullptr);
    if(thread)CloseHandle(thread);else logW("Museum JSON filter: watcher unavailable");
}
}
