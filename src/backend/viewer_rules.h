#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace integration {
inline bool utSourceCoBest(double probability,double best) {
    return std::isfinite(probability) && std::isfinite(best) && best>0 &&
        std::fabs(probability-best)<=best*1e-12;
}
template<class Rows,class Probability,class Match>
bool utBestSourcesMatch(const Rows& rows,Probability probability,Match match) {
    double best=0;
    for(const auto& row:rows)best=(std::max)(best,probability(row));
    for(size_t i=0;i<rows.size();++i)if(utSourceCoBest(probability(rows[i]),best) && match(i))return true;
    return false;
}
template<class Rows,class Probability>
size_t utSourceRetainCount(const Rows& sorted,Probability probability,size_t displayLimit=10) {
    size_t keep=(std::min)(sorted.size(),displayLimit);
    while(keep<sorted.size() && utSourceCoBest(probability(sorted[keep]),probability(sorted.front())))++keep;
    return keep;
}
template<class PrivateMatch,class PublicMatch>
bool utViewerSearchMatch(bool known,PrivateMatch privateMatch,PublicMatch publicMatch) {
    return (known && privateMatch()) || publicMatch();
}
inline int utViewerClampOffset(int offset,int total,int visible) {
    return (std::max)(0,(std::min)(offset,(std::max)(0,total-visible)));
}
struct ViewerPosition {
    int group=0,categoryTop=0,row=0;
    bool owned=false,sets=false,favorites=false;
    std::wstring query;
};
} // namespace integration
