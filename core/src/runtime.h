#pragma once
#include <windows.h>
#include <map>
#include <string>
#include "tqt_core.h"
namespace integration {
bool safeRead(const void*,void*,size_t);
inline void utGuardEnter() {}
inline void utGuardLeave() {}
void logI(const char*,...);
void logW(const char*,...);
const std::map<std::string,std::string>& tooltipFarmTargets();
bool tooltipFarmTargetsReady();
unsigned tooltipSourceRevision();
int tooltipSourceDifficulty();
int32_t TQM_CALL coreReady();
int32_t TQM_CALL coreDataDirectory(char*,uint32_t);
int32_t TQM_CALL coreSourceState(TqtSourceStateV1*);
int32_t TQM_CALL coreSourceText(uint32_t,char*,uint32_t,uint32_t*);
void TQM_CALL coreKeyboardBusy(uint32_t);
}
