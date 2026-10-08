#pragma once
#include "tqt_core.h"
#define TQT_RADAR_API_VERSION 1u
typedef struct TqtMobRadarApiV1 {
    uint32_t size,version;
    int32_t (TQM_CALL* isReady)(void);
    int32_t (TQM_CALL* setCollection)(const TqtCollectedItemV1*,uint32_t count,uint32_t known);
} TqtMobRadarApiV1;
typedef const TqtMobRadarApiV1* (TQM_CALL* TqtGetMobRadarApi)(uint32_t);
#ifdef __cplusplus
extern "C" {
#endif
const TqtMobRadarApiV1* TQM_CALL TQT_GetMobRadarApi(uint32_t version);
#ifdef __cplusplus
}
#endif
