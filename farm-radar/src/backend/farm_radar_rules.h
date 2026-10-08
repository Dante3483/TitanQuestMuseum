#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace museum {
inline int radarClampOffset(int offset,int total,int visible){return (std::max)(0,(std::min)(offset,(std::max)(0,total-visible)));}
struct RadarPoint {double x=0,y=0,z=0;int world=0;};
inline double radarDistanceSquared(const RadarPoint& a,const RadarPoint& b) {
    if(a.world!=b.world)return -1;
    const double x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;
    const double d=x*x+y*y+z*z;
    return std::isfinite(d)?d:-1;
}
struct RadarEntry {std::string name;double distanceSquared=0;};
inline void radarRemember(std::map<std::string,double>& nearest,const std::string& name,
                          double distanceSquared,double radius) {
    if(name.empty() || !std::isfinite(radius) || radius<=0 ||
       !std::isfinite(distanceSquared) || distanceSquared<0 || distanceSquared>radius*radius)return;
    auto inserted=nearest.emplace(name,distanceSquared);
    if(!inserted.second)inserted.first->second=(std::min)(inserted.first->second,distanceSquared);
}
inline std::vector<RadarEntry> radarSorted(const std::map<std::string,double>& nearest) {
    std::vector<RadarEntry> rows;
    for(const auto& entry:nearest)rows.push_back({entry.first,entry.second});
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){
        return a.distanceSquared!=b.distanceSquared?a.distanceSquared<b.distanceSquared:a.name<b.name;
    });
    return rows;
}
}
