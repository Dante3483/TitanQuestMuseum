#include "game/archive/loot_sources.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <set>
#include <map>
#include <vector>
#include <cmath>
#include <algorithm>
namespace {
int failures=0,checks=0;
void check(bool ok,const char* what){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",what);}}
struct Writer {
    std::string out="TQMLOOT2";
    void u32(unsigned v){for(int i=0;i<4;++i)out+=char(v>>(8*i));}
    void str(const std::string& s){u32(unsigned(s.size()));out+=s;}
    void num(const char* key,std::initializer_list<unsigned> values){str(key);u32(0);u32(unsigned(values.size()));for(auto v:values)u32(v);u32(0);}
    void text(const char* key,const std::string& value){str(key);u32(2);u32(1);u32(0);u32(1);str(value);}
    void monster(const std::string& key,const char* name,unsigned normal,unsigned epic,unsigned legendary,unsigned mask=7,const char* item="records/item/test.dbr"){
        str(key);str("Monster");u32(7);
        num("dropItems",{1});num("charLevel",{10,30,50});num("sourceDifficultyMask",{mask});
        num("chanceToEquipHead",{normal,epic,legendary});num("chanceToEquipHeadItem1",{100});
        text("lootHeadItem1",item);text("sourceName",name);
    }
};
void modelTest(){
    Writer w;w.u32(1);w.str("records/item/test.dbr");w.u32(0);w.u32(5);
    w.monster("records/creature/monster/alpha.dbr","Alpha",10,10,5);
    w.monster("records/creature/monster/beta.dbr","Beta",10,10,5);
    w.monster("records/creature/monster/lower.dbr","Lower",5,20,20);
    w.monster("records/creature/monster/alpha_variant.dbr","Alpha",1,1,1);
    w.monster("records/creature/monster/normal_only.dbr","Normal only",10,100,100,1);
    w.u32(5);for(const char* leaf:{"alpha","beta","lower","alpha_variant","normal_only"})
        w.str(std::string("records/creature/monster/")+leaf+".dbr");
    w.u32(0);
    const char* path="farm-radar-fixture.bin";
    {std::ofstream file(path,std::ios::binary);file.write(w.out.data(),std::streamsize(w.out.size()));}
    gen::LootSourceModel model;std::string error,output;std::map<std::string,std::string> targets;
    check(model.load(path,&error),"synthetic persisted loot model loads");
    check(model.calculate({10,10,10,1,0},output,&error,&targets),"normal context calculated");
    check(targets.size()==4 && targets.count("records/creature/monster/beta.dbr") &&
          !targets.count("records/creature/monster/lower.dbr"),"all exact co-best names included, lower source excluded");
    check(targets.count("records/creature/monster/alpha_variant.dbr")==1,"same-name variants follow Museum grouping");
    const std::string fullText=output;const auto fullTargets=targets;
    std::set<std::string> collected={"records/item/test.dbr"};
    check(model.calculate({10,10,10,1,0},output,&error,&targets,&collected)&&targets.empty(),"all best drops collected hides every source including ties");
    check(output==fullText,"radar filtering does not alter Museum probability rows");
    collected={"records/item/unrelated.dbr"};
    check(model.calculate({10,10,10,1,0},output,&error,&targets,&collected)&&targets==fullTargets,"unrelated collected item leaves all co-best sources visible");

    check(model.calculate({30,30,30,1,1},output,&error,&targets),"epic context calculated");
    check(targets.size()==1 && targets.begin()->second=="Lower","context changes ranking and excludes normal-only variants");
    check(!model.calculate({0,0,0,0,0},output,&error,&targets) && targets.empty(),"invalid context clears previous targets");
    std::remove(path);
}
void partialCollectionTest(){
    Writer w;w.u32(2);w.str("records/item/first.dbr");w.str("records/item/second.dbr");w.u32(0);w.u32(3);
    w.monster("records/creature/monster/shared_first.dbr","Shared",10,10,10,7,"records/item/first.dbr");
    w.monster("records/creature/monster/shared_second.dbr","Shared",10,10,10,7,"records/item/second.dbr");
    w.monster("records/creature/monster/completed.dbr","Completed",10,10,10,7,"records/item/first.dbr");
    w.u32(3);for(const char* leaf:{"shared_first","shared_second","completed"})w.str(std::string("records/creature/monster/")+leaf+".dbr");w.u32(0);
    const char* path="farm-partial-fixture.bin";{std::ofstream file(path,std::ios::binary);file.write(w.out.data(),std::streamsize(w.out.size()));}
    gen::LootSourceModel model;std::string error,text;std::map<std::string,std::string> targets;
    check(model.load(path,&error),"partial collection model loads");std::set<std::string> collected={"records/item/first.dbr"};
    check(model.calculate({10,10,10,1,0},text,&error,&targets,&collected)&&targets.size()==2&&!targets.count("records/creature/monster/completed.dbr"),
          "source remains for another missing best drop; fully completed source removed");
    collected.insert("records/item/second.dbr");
    check(model.calculate({10,10,10,1,0},text,&error,&targets,&collected)&&targets.empty(),"source disappears after last missing drop is collected");std::remove(path);
}
void installedModel(const char* path){
    gen::LootSourceModel model;std::string error,text;
    check(model.load(path,&error),"installed model loads");
    for(int difficulty=0;difficulty<3;++difficulty){
        std::map<std::string,std::string> targets;
        check(model.calculate({30,30,30,1,difficulty},text,&error,&targets),"installed context calculated");
        std::map<std::string,double> maximum;
        struct Row {std::string record,name;double p;};std::vector<Row> rows;
        std::istringstream stream(text);std::string line;std::getline(stream,line);
        while(std::getline(stream,line)){
            if(!line.empty()&&line.back()=='\r')line.pop_back();
            std::istringstream in(line);std::string record,kind,diff,level,prob,name;
            std::getline(in,record,'\t');std::getline(in,kind,'\t');std::getline(in,diff,'\t');
            std::getline(in,level,'\t');std::getline(in,prob,'\t');std::getline(in,name);
            const double p=std::stod(prob);maximum[record]=(std::max)(maximum[record],p);rows.push_back({record,name,p});
        }
        std::set<std::string> bestNames;
        // Text probabilities have 12 significant digits; allow serialization roundoff here only.
        for(const auto& row:rows)if(std::fabs(row.p-maximum[row.record])<=maximum[row.record]*1e-10)bestNames.insert(row.name);
        bool valid=!targets.empty();for(const auto& target:targets)
            if(!bestNames.count(target.second) || target.first.compare(0,8,"records/"))valid=false;
        check(valid,"installed targets agree with best Museum sources");
        std::printf("difficulty %d: %zu creature records, %zu best source names\n",difficulty,targets.size(),bestNames.size());
    }
}
}
int main(int argc,char** argv){modelTest();partialCollectionTest();if(argc>1)installedModel(argv[1]);
std::printf("Farm sources: %d checks, %d failures\n",checks,failures);return failures?1:0;}
