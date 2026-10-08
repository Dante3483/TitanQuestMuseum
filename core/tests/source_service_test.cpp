#include "tqt_core.h"
#include "game/archive/loot_sources.h"
#include <cstdio>
#include <cstring>
#include <vector>
namespace integration {void coreTestPublish(const gen::LootContext&,const std::string&,const std::map<std::string,std::string>&);}
namespace {int checks=0,failures=0;void check(bool value,const char* name){++checks;if(!value){++failures;std::printf("FAIL %s\n",name);}}}
int main(int argc,char** argv){
    check(TQT_GetCoreApi(0)==nullptr,"incompatible version refused");
    const auto* api=TQT_GetCoreApi(TQT_CORE_API_VERSION);
    check(api&&api->size==sizeof(*api)&&api->addons,"core and addon tables available without Museum");
    check(!api->isReady(),"consumers cannot use data before core initialization");
    TqtSourceStateV1 state={};check(!api->sourceState(&state),"incorrect state size refused");
    gen::LootContext context={19,19,19,1,0};std::string text="TQMSOURCES 2\r\n";
    std::map<std::string,std::string> targets;
    if(argc>1){gen::LootSourceModel model;std::string error;
        check(model.load(argv[1],&error)&&model.calculate(context,text,&error,&targets),"real model loaded and calculated by core");}
    integration::coreTestPublish(context,text,targets);
    state.size=sizeof(state);check(api->sourceState(&state)&&state.ready&&state.averageLevel==19&&state.difficulty==0,"live snapshot context published");
    unsigned count=0;check(api->copySourceText(state.revision,nullptr,0,&count)==TQM_COPY_OK&&count==text.size()+1,"caller queries source buffer size");
    std::vector<char> out(count,'X');
    check(api->copySourceText(state.revision,out.data(),count-1,&count)==TQM_COPY_CAPACITY&&out[0]=='X',"short buffer writes no bytes");
    check(api->copySourceText(state.revision,out.data(),count,&count)==TQM_COPY_OK&&!std::strcmp(out.data(),text.c_str()),"caller-owned buffer receives full source text");
    const unsigned old=state.revision;integration::coreTestPublish({20,20,20,1,0},"updated",targets);
    out[0]='X';check(api->copySourceText(old,out.data(),unsigned(out.size()),&count)==TQM_COPY_STALE&&out[0]=='X',"changed context refuses old revision without writing");
    char directory[64]={};check(api->copyDataDirectory(directory,sizeof(directory))&&!std::strcmp(directory,"fixture-core"),"shared database directory returned independently");
    integration::coreTestPublish({},"",{});state.size=sizeof(state);
    check(api->sourceState(&state)&&!state.ready&&state.difficulty==-1,"menu or invalid context clears source readiness");
    check(api->copySourceText(state.revision,nullptr,0,&count)==TQM_COPY_INVALID,"invalid context cannot expose old sources");
    check(!api->setCollection(nullptr,1,1),"missing collection buffer rejected");
    TqtCollectedItemV1 item={};strcpy_s(item.record,"Records\\Item\\test.dbr");
    check(api->setCollection(&item,1,1),"collected records normalized and accepted");
    state.size=sizeof(state);api->sourceState(&state);const unsigned filteredRevision=state.revision;
    strcpy_s(item.record,"records/item/test.dbr");
    check(api->setCollection(&item,1,1)&&api->sourceState(&state)&&state.revision==filteredRevision,"identical collection does not invalidate every frame");
    strcpy_s(item.record,"invalid");
    check(!api->setCollection(&item,1,1)&&api->sourceState(&state)&&state.revision==filteredRevision,"invalid snapshot preserves previous collection");
    check(api->setCollection(nullptr,0,0)&&api->sourceState(&state)&&state.revision!=filteredRevision,"unknown collection restores standalone mode and revises targets");

    std::printf("Core source service: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
