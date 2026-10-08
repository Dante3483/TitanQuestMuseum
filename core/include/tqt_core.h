#pragma once
#include "tqm_addon.h"
#ifdef __cplusplus
extern "C" {
#endif
#define TQT_CORE_API_VERSION 1u
typedef struct TqtSourceStateV1 {
    uint32_t size,revision,ready;
    int32_t averageLevel,minLevel,maxLevel,players,difficulty;
} TqtSourceStateV1;
typedef struct TqtCoreApiV1 {
    uint32_t size,version;
    const TqmAddonApiV1* addons;
    int32_t (TQM_CALL* isReady)(void);
    int32_t (TQM_CALL* copyDataDirectory)(char* output,uint32_t capacity);
    int32_t (TQM_CALL* sourceState)(TqtSourceStateV1* output);
    int32_t (TQM_CALL* copySourceText)(uint32_t revision,char* output,uint32_t capacity,uint32_t* bytes);
    /* Museum reports whether its search/viewer owns keyboard input. */
    void (TQM_CALL* setKeyboardBusy)(uint32_t busy);
} TqtCoreApiV1;
typedef const TqtCoreApiV1* (TQM_CALL* TqtGetCoreApi)(uint32_t version);
/* Exact export TQT_GetCoreApi on TitanQuestCore.asi. */
const TqtCoreApiV1* TQM_CALL TQT_GetCoreApi(uint32_t version);
#ifdef __cplusplus
}
#endif
