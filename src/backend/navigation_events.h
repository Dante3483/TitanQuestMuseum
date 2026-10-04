#pragma once
namespace integration {
struct UtNavBurst {
    int ticks;            // wheel ticks that moved the offset
    long long firstQpc;   // QPC of the first input since the last take, 0 = none
};
inline void utNavNote(UtNavBurst& b, bool wheel, long long qpc) {
    if (wheel) ++b.ticks;
    if (b.firstQpc == 0) b.firstQpc = qpc != 0 ? qpc : 1;
}
inline UtNavBurst utNavTake(UtNavBurst& b) {
    const UtNavBurst t = b;
    b.ticks = 0;
    b.firstQpc = 0;
    return t;
}


}