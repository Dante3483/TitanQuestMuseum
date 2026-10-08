#pragma once
#include <cstdint>
namespace radar {
struct FileStamp {
    uint64_t size=0,modified=0,identity=0;uint32_t volume=0;
    bool operator==(const FileStamp& other) const {
        return size==other.size&&modified==other.modified&&identity==other.identity&&volume==other.volume;
    }
};
class JournalCache {
    FileStamp stamp_;bool valid_=false;
public:
    bool needsRead(const FileStamp& stamp) const {return !valid_||!(stamp_==stamp);}
    void accept(const FileStamp& stamp){stamp_=stamp;valid_=true;}
    void forget(){valid_=false;}
};
}
