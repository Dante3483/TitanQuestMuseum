#pragma once
#include <map>
#include <string>
#include <fstream>
namespace museum::i18n {
inline std::map<std::string,std::string>& messages() {
    static std::map<std::string,std::string> data={
{"museum.section.equipment","Equipment"},
{"museum.section.weapons","Weapons"},
{"museum.section.other","Other"},
{"museum.search","Search"},
{"museum.search_off","Search off"},
{"museum.owned","Owned"},
{"museum.transfer","Transfer"},
{"museum.collection","Collection"},
{"museum.all","All"},
{"museum.found","found %d"},
{"museum.indexing","indexing %ld/%ld"},
{"museum.category.helm","Helm"},
{"museum.category.torso","Torso"},
{"museum.category.arms","Arms"},
{"museum.category.legs","Legs"},
{"museum.category.amulet","Amulet"},
{"museum.category.ring","Ring"},
{"museum.category.shield","Shield"},
{"museum.category.axe","Axe"},
{"museum.category.mace","Mace"},
{"museum.category.staff","Staff"},
{"museum.category.sword","Sword"},
{"museum.category.throw","Throw"},
{"museum.category.spear","Spear"},
{"museum.category.bow","Bow"},
{"museum.category.artifact","Artifact"}};
    return data;
}
inline const char* text(const char* key) {
    auto& data=messages(); auto i=data.find(key);
    return i==data.end()?key:i->second.c_str();
}
inline void load(const char* directory,const char* language) {
    const std::string lang=language?language:"EN";
    if (lang.empty() || lang.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos) return;
    for (const std::string& code : {std::string("EN"),lang}) {
        std::ifstream file(std::string(directory)+"/localization/"+code+".txt",std::ios::binary);
        std::string line;
        while (std::getline(file,line)) {
            if (!line.empty() && line.back()=='\r') line.pop_back();
            if (line.compare(0,3,"\xEF\xBB\xBF")==0) line.erase(0,3);
            const auto split=line.find('=');
            if (split==std::string::npos || line.compare(0,7,"museum.")!=0) continue;
            // Preserve printf argument types for translated status templates.
            const std::string key=line.substr(0,split),value=line.substr(split+1);
            auto signature=[](const std::string& s) {
                std::string result;
                for (size_t i=0;i<s.size();++i) if(s[i]=='%') {
                    if(i+1<s.size() && s[i+1]=='%') {++i;continue;}
                    size_t j=i+1;while(j<s.size() && (s[j]=='l' || s[j]=='d')) {result+=s[j];++j;if(s[j-1]=='d')break;}
                    if(j==i+1) result+='?';
                }
                return result;
            };
            auto old=messages().find(key);
            if(old!=messages().end() && signature(value)!=signature(old->second)) continue;
            messages()[key]=value;
        }
    }
}
}
