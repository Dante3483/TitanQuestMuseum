#pragma once
#include <set>
#include <string>
namespace radar {
inline std::string lootCaption(const std::string& record,const std::string& name,
                               const std::set<std::string>& collected,bool known){
    return known&&collected.count(record)?name:"???";
}
}
