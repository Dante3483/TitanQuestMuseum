#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include "backend/source_rules.h"

namespace integration {
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
