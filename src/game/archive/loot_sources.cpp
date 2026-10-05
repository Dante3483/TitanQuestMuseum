#include "game/archive/loot_sources.h"
#include "game/archive/arz_reader.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tuple>
#include <map>
#include <set>
#include <unordered_set>
#include <fstream>
#include <limits>

namespace gen { namespace {
bool begins(const std::string& s,const char* p) { return s.compare(0,std::char_traits<char>::length(p),p)==0; }
bool excludedSource(const std::string& key) {
    static const char* const parts[]={"/dev","/zzdev","/zz_dev","/sandbox/","/old/","/pets/","/pet/","/npc/","/e3 demo monsters/"};
    for(const char* part:parts)if(key.find(part)!=std::string::npos)return true;
    return false;
}
bool monsterRecord(const std::string& key,const ArzRecord& r) {
    const std::string cls=r.str("Class");
    if(cls=="Monster")return true;
    const bool creature=key.find("/creature/monster/")!=std::string::npos || key.find("/creatures/monster/")!=std::string::npos;
    return creature && r.find("charLevel") && r.find("description") &&
        (r.find("treasureProxyName") || r.find("dropItems") || r.find("dropMiscItems"));
}
double number(const ArzRecord& r,const std::string& key,int diff=0,double fallback=0) {
    const auto* f=r.find(key.c_str()); if(!f || f->raw.empty()) return fallback;
    const auto v=f->raw[(std::min)(size_t(diff),f->raw.size()-1)];
    if(f->type==kFtFloat) { float out;std::memcpy(&out,&v,4);return out; }
    return static_cast<int32_t>(v);
}
std::string string(const ArzRecord& r,const std::string& key,int diff=0) {
    const auto* f=r.find(key.c_str()); if(!f || f->strs.empty())return {};
    return f->strs[(std::min)(size_t(diff),f->strs.size()-1)];
}
// Deliberately small expression interpreter: no script execution and no unknown variable defaults.
struct Equation {
    const char* p;double parent;const LootContext& context;bool container=false,ok=true;
    void space(){while(*p==' ' || *p=='\t')++p;}
    double atom(){
        space();if(*p=='+' || *p=='-'){bool neg=*p++=='-';double v=atom();return neg?-v:v;}
        if(*p=='('){++p;double v=sum();space();if(*p!=')')ok=false;else ++p;return v;}
        if((*p>='0'&&*p<='9') || *p=='.'){char* end;double v=std::strtod(p,&end);p=end;return v;}
        std::string name;while((*p>='a'&&*p<='z') || (*p>='A'&&*p<='Z'))name+=*p++;
        if(name=="parentLevel" && !container)return parent;
        if(name=="averagePlayerLevel")return context.averageLevel;
        if(name=="minPlayerLevel")return container?context.minLevel:1;
        if(name=="maxPlayerLevel")return container?context.maxLevel:1;
        if(name=="numberOfPlayers")return container?context.players:1;
        if(name=="gameDifficulty")return context.difficulty;
        // proxyLevel needs the originating spawn proxy, not a fabricated boss level.
        ok=false;return 0;
    }
    double product(){double v=atom();space();while(*p=='*'||*p=='/'){char op=*p++;double b=atom();if(op=='*')v*=b;else if(b)v/=b;else ok=false;space();}return v;}
    double sum(){double v=product();space();while(*p=='+'||*p=='-'){char op=*p++;double b=product();v+=op=='+'?b:-b;space();}return v;}
};
bool evaluate(const std::string& s,int level,const LootContext& context,bool container,double& out){
    if(s.empty())return false;Equation e{s.c_str(),double(level),context,container};
    out=e.sum();e.space();return e.ok && !*e.p && std::isfinite(out);
}
using Drops=std::map<int,double>;
struct Node {std::string key,kind;ArzRecord r;};
std::string repeatChest(const std::map<std::string,Node>& nodes,const std::string& original) {
    if(original.empty())return {};
    const auto slash=original.rfind('/');if(slash==std::string::npos || original.size()<4)return original;
    const std::string file=original.substr(slash+1);
    if(begins(file,"repeat") || file.find("_repeat.dbr")!=std::string::npos)return original;
    const std::string prefix=original.substr(0,slash+1)+"repeat"+file;
    const std::string suffix=original.substr(0,original.size()-4)+"_repeat.dbr";
    if(nodes.count(prefix))return prefix;
    if(nodes.count(suffix))return suffix;
    // Some repeatable encounters use a single chest proxy without a separate variant.
    return original;
}
struct Pick {std::string name;int kind,diff,level;double p;};
struct Solver {
    std::vector<CatalogueItem> items;
    std::unordered_map<std::string,int> itemLevels;
    LootContext context;
    std::map<std::string,Node> nodes;
    std::unordered_map<std::string,int> ids;
    std::unordered_set<std::string> referenced;
    std::map<std::string,std::vector<std::string>> bossChests;
    std::map<std::string,Drops> memo;std::set<std::string> active;
    std::vector<std::vector<Pick>> best;int difficulty=0,level=1;size_t unknownEquations=0;
    Solver(const std::vector<CatalogueItem>& list):items(list),best(list.size()) {
        for(size_t i=0;i<items.size();++i)ids.emplace(ArzArchive::normKey(items[i].record),int(i));
    }
    void add(Drops& out,const Drops& d,double factor){for(const auto& v:d)out[v.first]+=factor*v.second;}
    Drops table(const std::string& path){
        std::string key=ArzArchive::normKey(path);
        const std::string memoKey=key+"#"+std::to_string(level);
        auto leaf=ids.find(key);if(leaf!=ids.end())return {{leaf->second,1}};
        auto old=memo.find(memoKey);if(old!=memo.end())return old->second;
        auto found=nodes.find(key);if(found==nodes.end() || active.size()>40 || !active.insert(key).second)return {};
        const Node& n=found->second;const ArzRecord& r=n.r;Drops out;
        if(n.kind=="LootMasterTable" || n.kind=="LootItemTable_FixedWeight") {
            double total=0;for(const auto& f:r.fields)if(begins(f.name,"lootWeight"))total+=(std::max)(0.0,number(r,f.name,difficulty));
            if(total>0)for(const auto& f:r.fields)if(begins(f.name,"lootName")) {
                const double w=number(r,"lootWeight"+f.name.substr(8),difficulty);
                if(w>0)add(out,table(string(r,f.name,difficulty)),w/total);
            }
        } else if(n.kind=="LootItemTable_DynWeight") {
            double lo=0,hi=0,target=0;
            if(!evaluate(r.str("minItemLevelEquation"),level,context,false,lo) || !evaluate(r.str("maxItemLevelEquation"),level,context,false,hi)
               || !evaluate(r.str("targetLevelEquation"),level,context,false,target)){++unknownEquations;}
            else {
                // Game.dll SetValidItemLevel falls back to the highest item below the minimum.
                const auto* names=r.find("itemNames");const auto* slope=r.find("bellSlope");
                std::vector<std::pair<std::string,int>> eligible,fallback;int nearest=-1;
                int minLevel=int(lo+0.5),maxLevel=int(hi+0.5),targetLevel=int(target+0.5);
                int availableMin=std::numeric_limits<int>::max(),availableMax=0;
                if(names)for(const auto& item:names->strs){auto entry=itemLevels.find(ArzArchive::normKey(item));if(entry==itemLevels.end())continue;
                    availableMin=(std::min)(availableMin,entry->second);availableMax=(std::max)(availableMax,entry->second);}
                if(availableMin<=availableMax){
                    minLevel=(std::max)(availableMin,(std::min)(availableMax,minLevel));
                    maxLevel=(std::max)(availableMin,(std::min)(availableMax,maxLevel));
                    targetLevel=(std::max)(availableMin,(std::min)(availableMax,targetLevel));
                }
                if(names)for(const auto& item:names->strs){auto entry=itemLevels.find(ArzArchive::normKey(item));if(entry==itemLevels.end())continue;
                    int il=entry->second;if(il>=minLevel && il<=maxLevel)eligible.emplace_back(item,il);
                    if(il<minLevel && il>=nearest){if(il>nearest){nearest=il;fallback.clear();}fallback.emplace_back(item,il);}}
                if(eligible.empty())eligible=fallback;
                double total=0;std::vector<std::pair<std::string,double>> weights;
                for(const auto& item:eligible){double w=number(r,"defaultWeight",0,1);int distance=std::abs(item.second-targetLevel);
                    if(slope && size_t(distance)<slope->raw.size())w=std::floor(w*int(number(r,"bellSlope",distance))/100.0);
                    if(w>0){weights.emplace_back(item.first,w);total+=w;}}
                if(total>0)for(const auto& item:weights)add(out,table(item.first),item.second/total);
            }
        } else if(n.kind=="FixedItemContainer") {
            // FixedItemController::PickLootRecord selects ONE table by the computed
            // container-level index (clamped to the final entry), not all tables.
            const auto* f=r.find("tables");if(f&&!f->strs.empty()){
                int index=0;const std::string equationPath=r.str("levelEquationFile");
                if(!equationPath.empty()){double result=0;
                    if(!evaluate(r.str("resolvedLevelEquation"),level,context,true,result)){
                        ++unknownEquations;active.erase(key);memo.emplace(memoKey,out);return out;
                    }
                    index=int((std::max)(0.0,result)+0.5);
                }
                out=table(f->strs[(std::min)(size_t(index),f->strs.size()-1)]);
            }
        } else if(n.kind=="FixedItemLoot") {
            // SelectLoot chooses one of six groups by normalized Chance weights.
            // SelectLootNumber rounds equation bounds, then rounds a uniform real draw.
            double a=0,b=0;if(!evaluate(r.str("numSpawnMinEquation"),level,context,true,a)||!evaluate(r.str("numSpawnMaxEquation"),level,context,true,b))++unknownEquations;
            else if(a>=0 && b>=a && b<=100){
                const int low=int(a+0.5),high=int(b+0.5);
                double groupTotal=0;for(int slot=1;slot<=6;++slot)
                    groupTotal+=(std::max)(0.0,number(r,"loot"+std::to_string(slot)+"Chance",difficulty));
                Drops draw;
                // LoadDropLoot passes the selected FixedItemLoot's goldGeneratorLevel to SelectLoot.
                const int outerLevel=level;level=int(number(r,"goldGeneratorLevel"));
                for(int slot=1;slot<=6;++slot){std::string prefix="loot"+std::to_string(slot);
                    const double group=number(r,prefix+"Chance",difficulty);
                    double total=0;for(int i=1;i<=6;++i)total+=(std::max)(0.0,number(r,prefix+"Weight"+std::to_string(i),difficulty));
                    if(groupTotal>0 && total>0 && group>0)for(int i=1;i<=6;++i){double w=number(r,prefix+"Weight"+std::to_string(i),difficulty);
                        if(w>0)add(draw,table(string(r,prefix+"Name"+std::to_string(i),difficulty)),group/groupTotal*w/total);}
                }
                level=outerLevel;
                for(const auto& v:draw){double none=0;const double p=(std::min)(1.0,v.second);
                    if(low==high)none=std::pow(1-p,low);
                    else for(int count=low;count<=high;++count){
                        const double mass=(count==low||count==high?0.5:1.0)/(high-low);
                        none+=mass*std::pow(1-p,count);
                    }
                    out[v.first]=1-none;
                }
            }
        } else if(n.kind=="ProxyAccessoryPool") {
            double total=0;for(const auto& f:r.fields)if(begins(f.name,"fixedItemWeight"))total+=(std::max)(0.0,number(r,f.name,difficulty));
            if(total>0)for(const auto& f:r.fields)if(begins(f.name,"fixedItemName")){double w=number(r,"fixedItemWeight"+f.name.substr(13),difficulty);
                if(w>0)add(out,table(string(r,f.name,difficulty)),w/total*number(r,"fixedItemChance",difficulty,100)/100.0);}
        }
        active.erase(key);memo.emplace(memoKey,out);return out;
    }
    std::string name(const ArzRecord& r){return r.str("sourceName");}
    void submit(const Drops& d,const std::string& source,int kind) {
        if(source.empty())return;
        for(const auto& v:d){if(v.second<=0)continue;Pick p{source,kind,difficulty,level,(std::min)(1.0,v.second)};
            auto& list=best[v.first];auto same=std::find_if(list.begin(),list.end(),[&](const Pick& x){return x.name==p.name&&x.kind==p.kind&&x.diff==p.diff;});
            if(same!=list.end()){if(same->p>=p.p)continue;*same=p;}else list.push_back(p);
            std::sort(list.begin(),list.end(),[](const Pick& x,const Pick& y){if(x.p!=y.p)return x.p>y.p;return std::tie(x.name,x.diff,x.kind)<std::tie(y.name,y.diff,y.kind);});
            if(list.size()>10)list.resize(10);
        }
    }
    void monster(const Node& n){
        static const char* const slots[]={"Head","Torso","LowerBody","Forearm","Finger1","Finger2","RightHand","LeftHand","Misc1","Misc2","Misc3"};
        Drops equip,loot;
        for(const char* slot:slots){
            const bool misc=begins(slot,"Misc");
            if(number(n.r,misc?"dropMiscItems":"dropItems",0,misc?1:0)!=1)continue;std::string prefix="chanceToEquip"+std::string(slot);double chance=number(n.r,prefix,difficulty)/100.0;
            double total=0;for(int i=1;i<=6;++i)total+=(std::max)(0.0,number(n.r,prefix+"Item"+std::to_string(i),difficulty));
            Drops one;if(total>0&&chance>0)for(int i=1;i<=6;++i){double w=number(n.r,prefix+"Item"+std::to_string(i),difficulty);if(w>0)
                add(one,table(string(n.r,"loot"+std::string(slot)+"Item"+std::to_string(i),difficulty)),chance*w/total);}
            Drops& out=begins(slot,"Misc")?loot:equip;
            for(const auto& v:one)out[v.first]=1-(1-out[v.first])*(1-v.second);
        }
        const std::string label=name(n.r);submit(equip,label,1);submit(loot,label,0);
        std::vector<std::string> proxies=bossChests[n.key];
        const std::string direct=repeatChest(nodes,ArzArchive::normKey(string(n.r,"treasureProxyName",difficulty)));
        if(!direct.empty() && std::find(proxies.begin(),proxies.end(),direct)==proxies.end())proxies.push_back(direct);
        for(const auto& path:proxies){auto proxy=nodes.find(path);if(proxy==nodes.end())continue;
            Drops chest;const std::string prefix=difficulty==0?"accessory":difficulty==1?"accessoryEpic":"accessoryLegendary";
            for(const auto& f:proxy->second.r.fields)if(begins(f.name,prefix.c_str()) && f.name.size()>prefix.size()
                && f.name[prefix.size()]>='0'&&f.name[prefix.size()]<='9'){
                Drops one=table(string(proxy->second.r,f.name));for(const auto& v:one)chest[v.first]=1-(1-chest[v.first])*(1-v.second);
            }
            submit(chest,label,2);
        }

    }
};

struct Writer {
    std::string& out;
    void u32(uint32_t v){for(int i=0;i<4;++i)out+=char(v>>(i*8));}
    void str(const std::string& v){u32(uint32_t(v.size()));out+=v;}
};
struct Reader {
    const std::string& in;size_t pos=0;bool ok=true;
    uint32_t u32(){if(pos+4>in.size()){ok=false;return 0;}uint32_t v=0;for(int i=0;i<4;++i)v|=uint32_t(uint8_t(in[pos++]))<<(i*8);return v;}
    uint32_t count(uint32_t limit){uint32_t n=u32();if(n>limit){ok=false;return 0;}return n;}
    std::string str(){uint32_t n=count(16384);if(!ok||n>in.size()-pos){ok=false;return {};}std::string s=in.substr(pos,n);pos+=n;return s;}
};
void serializeModel(const Solver& s,std::string& output){
    output="TQMLOOT2";Writer w{output};w.u32(uint32_t(s.items.size()));
    for(const auto& item:s.items)w.str(item.record);
    w.u32(uint32_t(s.itemLevels.size()));
    // Ordered serialization keeps regeneration deterministic.
    std::map<std::string,int> levels(s.itemLevels.begin(),s.itemLevels.end());
    for(const auto& v:levels){w.str(v.first);w.u32(uint32_t(v.second));}
    w.u32(uint32_t(s.nodes.size()));
    for(const auto& entry:s.nodes){const Node& n=entry.second;w.str(n.key);w.str(n.kind);w.u32(uint32_t(n.r.fields.size()));
        for(const auto& f:n.r.fields){w.str(f.name);w.u32(f.type);w.u32(uint32_t(f.raw.size()));for(auto v:f.raw)w.u32(v);
            w.u32(uint32_t(f.strs.size()));for(const auto& v:f.strs)w.str(v);}}
    std::set<std::string> referenced(s.referenced.begin(),s.referenced.end());w.u32(uint32_t(referenced.size()));for(const auto& v:referenced)w.str(v);
    w.u32(uint32_t(s.bossChests.size()));for(const auto& entry:s.bossChests){w.str(entry.first);w.u32(uint32_t(entry.second.size()));for(const auto& v:entry.second)w.str(v);}
}
void runSolver(Solver& solver,bool currentDifficulty){
    solver.best.assign(solver.items.size(),{});solver.unknownEquations=0;
    std::map<std::pair<int,int>,std::vector<const Node*>> contexts;
    for(const auto& entry:solver.nodes){const Node& n=entry.second;if(n.kind!="Monster"||!solver.referenced.count(n.key)||solver.name(n.r).empty())continue;
        if(excludedSource(n.key))continue;
        const auto* levels=n.r.find("charLevel");if(!levels||levels->raw.empty())continue;
        for(int d=0;d<3;++d){if(currentDifficulty&&d!=solver.context.difficulty)continue;
            const int lev=int(number(n.r,"charLevel",d));if(lev<1||lev>120)continue;
            if(!(int(number(n.r,"sourceDifficultyMask",0,7)) & (1<<d)))continue;
            contexts[{d,lev}].push_back(&n);}
    }
    const int wantedDifficulty=solver.context.difficulty;
    for(const auto& c:contexts){solver.difficulty=c.first.first;solver.context.difficulty=solver.difficulty;solver.level=c.first.second;solver.memo.clear();solver.active.clear();
        for(const Node* n:c.second)solver.monster(*n);}
    solver.context.difficulty=wantedDifficulty;solver.memo.clear();
}
void formatSources(const Solver& solver,std::string& output){
    output="TQMSOURCES 2\r\n";
    for(size_t i=0;i<solver.items.size();++i)for(const auto& p:solver.best[i]){
        std::string clean=p.name;for(char& c:clean)if(c=='\t'||c=='\r'||c=='\n')c=' ';
        char fields[96];std::snprintf(fields,sizeof(fields),"\t%d\t%d\t%d\t%.12g\t",p.kind,p.diff,p.level,p.p);
        output+=solver.items[i].record+fields+clean+"\r\n";}
}
} // namespace
bool buildLootSources(const GameData& g,const std::vector<CatalogueItem>& items,std::string& output,
                      std::vector<std::string>& warnings,std::string* error,std::string* modelOutput){
    output="TQMSOURCES 2\r\n";Solver solver(items);ArzRecord rec;
    size_t excluded=0,missingNames=0,unsupportedContainers=0;
    for(const auto& winner:g.db->winners()){
        const auto& entry=*winner.second;
        if(!g.db->archives()[winner.first].decode(entry,rec,error))return false;
        if(rec.find("itemLevel"))solver.itemLevels[entry.key]=rec.i32("itemLevel");
        for(const auto& f:rec.fields)for(const auto& ref:f.strs)if(ref.size()>4 && ArzArchive::normKey(ref).compare(0,8,"records/")==0)
            solver.referenced.insert(ArzArchive::normKey(ref));
        if(rec.str("Class")=="FixedItemContainerTartarus")++unsupportedContainers;
        std::string kind=rec.str("Class"),tmpl=ArzArchive::normKey(rec.str("templateName"));
        if(monsterRecord(entry.key,rec))kind="Monster";
        if(tmpl.find("fixeditemloot.tpl")!=std::string::npos)kind="FixedItemLoot";
        if(tmpl.find("proxypool.tpl")!=std::string::npos)kind="ProxyPool";
        if(kind!="Monster"&&kind!="Proxy"&&kind!="ProxyPool"&&kind!="ProxyAccessoryPool"&&kind!="FixedItemContainer"
            &&kind!="FixedItemLoot"&&kind!="LootMasterTable"&&kind!="LootItemTable_FixedWeight"&&kind!="LootItemTable_DynWeight")continue;
        Node n{entry.key,kind,{}};
        for(auto& f:rec.fields)if(begins(f.name,"pool")||begins(f.name,"name")||begins(f.name,"loot")||begins(f.name,"chanceToEquip")||begins(f.name,"accessory")||begins(f.name,"fixedItem")
            ||f.name=="description"||f.name=="dropItems"||f.name=="dropMiscItems"||f.name=="charLevel"||f.name=="treasureProxyName"||f.name=="tables"
            ||f.name=="levelEquationFile"||f.name=="numSpawnMinEquation"||f.name=="numSpawnMaxEquation"||f.name=="minItemLevelEquation"||f.name=="maxItemLevelEquation"
            ||f.name=="targetLevelEquation"||f.name=="itemNames"||f.name=="bellSlope"||f.name=="defaultWeight"||f.name=="goldGeneratorLevel")n.r.fields.push_back(std::move(f));
        std::string description=n.r.str("description");
        const auto end=description.find_last_not_of(" \t\r\n");
        description=end==std::string::npos?std::string():description.substr(0,end+1);
        auto tag=g.tags.find(description);
        if(tag!=g.tags.end()){ArzField f;f.name="sourceName";f.type=kFtString;f.raw.push_back(0);f.strs.push_back(tag->second);n.r.fields.push_back(std::move(f));}
        if(n.kind=="Monster") {
            if(excludedSource(n.key))++excluded;
            else if(tag==g.tags.end())++missingNames;
        }
        const std::string equationPath=n.r.str("levelEquationFile");
        if(!equationPath.empty()){ArzRecord eq;if(g.db->get(equationPath,eq)){
            ArzField f;f.name="resolvedLevelEquation";f.type=kFtString;f.raw.push_back(0);f.strs.push_back(eq.str("levelEquation"));n.r.fields.push_back(std::move(f));}}
        solver.nodes.emplace(n.key,std::move(n));
    }
    // Resolve difficulty from actual proxy -> pool -> monster links, never equal levels.
    std::map<std::string,unsigned> masks;
    const auto visit=[&](const auto& self,const std::string& path,int difficulty,std::set<std::string>& seen)->void {
        const auto key=ArzArchive::normKey(path);auto found=solver.nodes.find(key);
        if(found==solver.nodes.end() || seen.size()>256 || !seen.insert(key).second)return;
        const Node& node=found->second;
        if(node.kind=="Monster"){masks[key]|=1u<<difficulty;return;}
        if(node.kind=="ProxyPool") {
            for(const auto& f:node.r.fields)if(begins(f.name,"name"))for(const auto& v:f.strs)self(self,v,difficulty,seen);
        } else if(node.kind=="Proxy") {
            const std::string overridePrefix=difficulty==1?"poolEpic":difficulty==2?"poolLegendary":"pool";
            bool override=false;
            if(difficulty)for(const auto& f:node.r.fields)if(begins(f.name,overridePrefix.c_str())&&!f.strs.empty())override=true;
            const std::string prefix=override?overridePrefix:"pool";
            for(const auto& f:node.r.fields)if(begins(f.name,prefix.c_str())&&f.name.size()>prefix.size()
                && f.name[prefix.size()]>='0'&&f.name[prefix.size()]<='9')
                for(const auto& v:f.strs)self(self,v,difficulty,seen);
        }
    };
    for(const auto& entry:solver.nodes)if(entry.second.kind=="Proxy"&&!excludedSource(entry.first))
        for(int d=0;d<3;++d){std::set<std::string> seen;visit(visit,entry.first,d,seen);}
    for(const auto& v:masks){ArzField field;field.name="sourceDifficultyMask";field.type=kFtInt;field.raw.push_back(v.second);solver.nodes.at(v.first).r.fields.push_back(std::move(field));}
    // Quest bosses can have their reward chest spawned externally, with no monster backlink.
    // Resolve named boss spawn proxies -> pool members; only explicit boss paths are accepted.
    const std::map<std::string,std::vector<std::string>> aliases={
        {"gorgons",{"medusa","sstheno","euryale"}},
        {"pharaohshonorguard",{"pharaoh'shonorguard"}},
        {"ancientlimos",{"ancientlimos"}}, {"ancientscorpos",{"ancientscorpos"}},
        {"ancienttropicalarachnos",{"ancienttropicalarachnos"}}};
    for(const auto& chest:solver.nodes){
        if(!begins(chest.first,"records/item/containers/boss/proxy/"))continue;
        std::string file=chest.first.substr(chest.first.rfind('/')+1);
        const bool suffixRepeat=file.size()>11 && file.compare(file.size()-11,11,"_repeat.dbr")==0;
        const bool repeat=begins(file,"repeatbosschestproxy") || (begins(file,"bosschestproxy") && suffixRepeat);
        if(!repeat)continue; // farming: explicitly repeatable boss chests only
        size_t sep=file.find('_');if(sep==std::string::npos)continue;
        std::string token=file.substr(sep+1,file.size()-sep-5);
        if(suffixRepeat)token.resize(token.size()-7);
        if(token.find('_')!=std::string::npos)continue; // separate special quest variants
        std::vector<std::string> tokens={token};auto alias=aliases.find(token);if(alias!=aliases.end())tokens=alias->second;
        for(const auto& spawn:solver.nodes){
            if(!begins(spawn.first,"records/proxies boss/boss/bossproxy_"))continue;
            std::string base=spawn.first.substr(spawn.first.rfind('/')+1);size_t start=base.find('_',10);
            if(start==std::string::npos)continue;std::string boss=base.substr(start+1,base.size()-start-5);
            if(boss.size()>8&&boss.compare(boss.size()-8,8,"_telkine")==0)boss.resize(boss.size()-8);
            if(boss.size()>6&&boss.compare(boss.size()-6,6,"_titan")==0)boss.resize(boss.size()-6);
            if(std::find(tokens.begin(),tokens.end(),boss)==tokens.end())continue;
            for(const auto& field:spawn.second.r.fields)if(begins(field.name,"pool"))for(const auto& poolPath:field.strs){
                auto pool=solver.nodes.find(ArzArchive::normKey(poolPath));if(pool==solver.nodes.end()||pool->second.kind!="ProxyPool")continue;
                for(const auto& member:pool->second.r.fields)if(begins(member.name,"name"))for(const auto& monsterPath:member.strs){
                    const std::string key=ArzArchive::normKey(monsterPath);auto node=solver.nodes.find(key);
                    if(node==solver.nodes.end()||node->second.kind!="Monster")continue;
                    auto& paths=solver.bossChests[key];if(std::find(paths.begin(),paths.end(),chest.first)==paths.end())paths.push_back(chest.first);
                }
            }
        }
    }
    // Retain the full reachable source graph, not every unrelated proxy/developer table.
    // This prunes storage only; all supported source candidates still participate in ranking.
    std::set<std::string> keep;std::vector<std::string> queue;
    auto enqueue=[&](const std::string& path){std::string key=ArzArchive::normKey(path);if(solver.nodes.count(key)&&keep.insert(key).second)queue.push_back(key);};
    for(const auto& entry:solver.nodes){const Node& n=entry.second;
        if(n.kind!="Monster"||!solver.referenced.count(n.key)||solver.name(n.r).empty()||!n.r.find("charLevel"))continue;
        if(excludedSource(n.key))continue;
        enqueue(n.key);auto chests=solver.bossChests.find(n.key);if(chests!=solver.bossChests.end())for(const auto& path:chests->second)enqueue(path);
        for(int d=0;d<3;++d)enqueue(repeatChest(solver.nodes,ArzArchive::normKey(string(n.r,"treasureProxyName",d))));
    }
    for(size_t i=0;i<queue.size();++i){const auto& node=solver.nodes.at(queue[i]);
        for(const auto& f:node.r.fields)for(const auto& path:f.strs)if(path.size()>8&&ArzArchive::normKey(path).compare(0,8,"records/")==0)enqueue(path);}
    for(auto it=solver.nodes.begin();it!=solver.nodes.end();)if(!keep.count(it->first))it=solver.nodes.erase(it);else ++it;
    for(auto it=solver.referenced.begin();it!=solver.referenced.end();)if(!solver.nodes.count(*it)||solver.nodes.at(*it).kind!="Monster")it=solver.referenced.erase(it);else ++it;
    std::set<std::string> levelKeys;
    for(const auto& entry:solver.nodes){const auto* names=entry.second.r.find("itemNames");if(names)for(const auto& path:names->strs)levelKeys.insert(ArzArchive::normKey(path));}
    for(auto it=solver.itemLevels.begin();it!=solver.itemLevels.end();)if(!levelKeys.count(it->first))it=solver.itemLevels.erase(it);else ++it;
    if(modelOutput)serializeModel(solver,*modelOutput);
    // Offline reference is explicitly level 10 solo, not source level substituted for player.
    solver.context={10,10,10,1,0};
    runSolver(solver,false);
    size_t covered=0;
    for(const auto& best:solver.best)if(!best.empty())++covered;
    formatSources(solver,output);
    warnings.push_back("loot sources: "+std::to_string(covered)+"/"+std::to_string(items.size())+" records; conditional solo reference, player level 10; runtime uses live context");
    warnings.push_back("loot source audit: "+std::to_string(solver.referenced.size())+" retained creature records; "+std::to_string(excluded)+" internal/pet/NPC records excluded; "+std::to_string(missingNames)+" creature names unresolved before reachability pruning");
    if(unsupportedContainers)warnings.push_back("loot source audit: "+std::to_string(unsupportedContainers)+" Tartarus containers unsupported; no generic chest probability substituted");
    if(solver.unknownEquations)warnings.push_back("loot sources: unsupported equations skipped ("+std::to_string(solver.unknownEquations)+")");
    return true;
}

bool LootContext::valid() const {return minLevel>=1&&maxLevel<=120&&minLevel<=averageLevel&&averageLevel<=maxLevel&&players>=1&&players<=16&&difficulty>=0&&difficulty<=2;}
bool LootContext::operator==(const LootContext& c) const {return std::tie(averageLevel,minLevel,maxLevel,players,difficulty)==std::tie(c.averageLevel,c.minLevel,c.maxLevel,c.players,c.difficulty);}
struct LootSourceModel::Impl {std::unique_ptr<Solver> solver;};
LootSourceModel::LootSourceModel():m_impl(new Impl){}
LootSourceModel::~LootSourceModel()=default;
bool LootSourceModel::load(const std::string& path,std::string* error){
    std::ifstream file(path,std::ios::binary|std::ios::ate);auto fail=[&](){if(error)*error="missing or invalid loot model";return false;};
    if(!file)return fail();const auto size=file.tellg();if(size<8||size>64*1024*1024)return fail();
    std::string data(size_t(size),'\0');file.seekg(0);if(!file.read(&data[0],size))return fail();
    if(data.compare(0,8,"TQMLOOT2")!=0)return fail();Reader r{data,8};
    std::vector<CatalogueItem> items;uint32_t n=r.count(4096);for(uint32_t i=0;i<n&&r.ok;++i){CatalogueItem item;item.record=r.str();items.push_back(std::move(item));}
    std::unique_ptr<Solver> s(new Solver(items));n=r.count(100000);
    for(uint32_t i=0;i<n&&r.ok;++i){std::string k=r.str();const auto v=r.u32();s->itemLevels.emplace(std::move(k),int(v));}
    n=r.count(100000);for(uint32_t i=0;i<n&&r.ok;++i){Node node;node.key=r.str();node.kind=r.str();uint32_t fields=r.count(4096);
        for(uint32_t j=0;j<fields&&r.ok;++j){ArzField f;f.name=r.str();f.type=uint16_t(r.u32());uint32_t a=r.count(4096);for(uint32_t x=0;x<a&&r.ok;++x)f.raw.push_back(r.u32());
            a=r.count(4096);for(uint32_t x=0;x<a&&r.ok;++x)f.strs.push_back(r.str());node.r.fields.push_back(std::move(f));}
        s->nodes.emplace(node.key,std::move(node));}
    n=r.count(200000);for(uint32_t i=0;i<n&&r.ok;++i)s->referenced.insert(r.str());
    n=r.count(100000);for(uint32_t i=0;i<n&&r.ok;++i){std::string k=r.str();uint32_t a=r.count(1024);auto& paths=s->bossChests[k];for(uint32_t x=0;x<a&&r.ok;++x)paths.push_back(r.str());}
    if(!r.ok||r.pos!=data.size()||items.empty())return fail();m_impl->solver=std::move(s);return true;
}
bool LootSourceModel::calculate(const LootContext& context,std::string& output,std::string* error){
    output.clear();if(!m_impl->solver||!context.valid()){if(error)*error="unavailable loot context";return false;}
    Solver& s=*m_impl->solver;s.context=context;runSolver(s,true);formatSources(s,output);return true;
}
} // namespace gen
