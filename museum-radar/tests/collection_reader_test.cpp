#include "backend/journal_collection.h"
#include "backend/file_stamp.h"
#include "backend/loot_identity.h"
#include <cstdio>
#include <fstream>
#include <iterator>
namespace {int checks=0,failures=0;void check(bool ok,const char* message){++checks;if(!ok){++failures;std::printf("FAIL %s\n",message);}}
std::string row(const char* pending){
    return std::string("{\"record\":\"records/item/test.dbr\",\"base\":\"Records\\\\Item\\\\test.dbr\",\"prefix\":\"\",\"suffix\":\"\",\"relic\":\"\",\"relicBonus\":\"\",\"relic2\":\"\",\"relicBonus2\":\"\",\"seed\":1,\"var1\":0,\"var2\":0,\"stack\":1,\"b8\":0")+
        (pending[0]?std::string(",\"pending\":\"")+pending+"\"":"")+"}\n";
}}
int main(int argc,char** argv){
    radar::JournalCache cache;
    radar::FileStamp initial{100,200,300,1};
    check(cache.needsRead(initial),"first snapshot requires read");
    cache.accept(initial);
    check(!cache.needsRead(initial),"unchanged snapshot skips contents read");
    auto next=initial;next.modified++;
    check(cache.needsRead(next),"changed timestamp requires read");
    next=initial;next.size++;
    check(cache.needsRead(next),"changed size requires read");
    next=initial;next.identity++;
    check(cache.needsRead(next),"atomic replacement with same size and timestamp requires read");
    cache.forget();
    check(cache.needsRead(initial),"reappearing file requires read");
    const std::set<std::string> owned={"records/item/known.dbr"};
    check(radar::lootCaption("records/item/known.dbr","Known item",owned,true)=="Known item","known real loot reveals its name");
    check(radar::lootCaption("records/item/missing.dbr","Secret name",owned,true)=="???","unknown real loot conceals its name");
    check(radar::lootCaption("records/item/known.dbr","Known item",owned,false)=="???","unavailable collection never reveals cached identities");
    const std::string header="{\"journal\":\"titan quest uniquetab\",\"format\":1,\"set\":\"tq-uniq-items\",\"entries\":3,\"collected\":2,\"pendingIn\":1,\"pendingOut\":1}\n";
    const std::string text=header+row("")+row("in")+row("out");std::set<std::string> keys;
    check(radar::collectedJournal(text,keys)&&keys.size()==1&&keys.count("records/item/test.dbr"),"copies deduplicated; pending in counts, pending out does not");
    check(!radar::collectedJournal(text.substr(0,text.size()-5),keys)&&keys.empty(),"partial file never hides collected items");
    auto altered=text;altered.replace(altered.find("\"format\":1"),10,"\"format\":2");
    check(!radar::collectedJournal(altered,keys),"unsupported journal version refused");
    altered=text;altered.replace(altered.find("\"entries\":3"),11,"\"entries\":4");
    check(!radar::collectedJournal(altered,keys),"row count mismatch refused");
    altered=text;altered.replace(altered.find("\"seed\":1"),8,"\"seed\":null");
    check(!radar::collectedJournal(altered,keys),"invalid item identity refused");
    if(argc>1){std::ifstream file(argv[1],std::ios::binary);std::string actual((std::istreambuf_iterator<char>(file)),{});
        check(!actual.empty()&&radar::collectedJournal(actual,keys),"real Museum JSON loads without Museum runtime or database");
        std::printf("real journal: %zu collected records\n",keys.size());}
    std::printf("Radar JSON reader: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
