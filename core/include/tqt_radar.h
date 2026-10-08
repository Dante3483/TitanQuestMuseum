#pragma once
#include "tqt_core.h"
#define TQT_RADAR_API_VERSION 2u
typedef struct TqtMobRadarApiV1 {
    uint32_t size,version;
    int32_t (TQM_CALL* isReady)(void);
    int32_t (TQM_CALL* setCollection)(const TqtCollectedItemV1*,uint32_t count,uint32_t known);
} TqtMobRadarApiV1;
/* Borrowed engine pointers are valid only during inspect. No actor may be retained. */
typedef struct TqtNearbyMobV1 {const void* actor;char record[512];double distanceSquared;} TqtNearbyMobV1;
typedef struct TqtRadarScanV1 {
    uint32_t size;const void* const* objects;uint32_t objectCount;
    const TqtNearbyMobV1* mobs;uint32_t mobCount;
} TqtRadarScanV1;
typedef struct TqtRadarLootV1 {uint32_t mobIndex;char mobName[1024],itemName[1024];} TqtRadarLootV1;
typedef uint32_t (TQM_CALL* TqtInspectLootV1)(void* user,const TqtRadarScanV1*,TqtRadarLootV1*,uint32_t capacity);
typedef struct TqtRadarInspectorV1 {uint32_t size;void* user;TqtInspectLootV1 inspect;} TqtRadarInspectorV1;
typedef struct TqtMobRadarApiV2 {
    uint32_t size,version;
    int32_t (TQM_CALL* isReady)(void);
    int32_t (TQM_CALL* setCollection)(const TqtCollectedItemV1*,uint32_t count,uint32_t known);
    int32_t (TQM_CALL* copyDataDirectory)(char*,uint32_t);
    int32_t (TQM_CALL* registerInspector)(const TqtRadarInspectorV1*);
} TqtMobRadarApiV2;
typedef const void* (TQM_CALL* TqtGetMobRadarApi)(uint32_t);
#ifdef __cplusplus
extern "C" {
#endif
const void* TQM_CALL TQT_GetMobRadarApi(uint32_t version);
#ifdef __cplusplus
}
#if defined(_M_IX86)
static_assert(sizeof(TqtMobRadarApiV1)==16,"legacy radar API ABI");
static_assert(sizeof(TqtMobRadarApiV2)==24,"radar inspector API ABI");
static_assert(sizeof(TqtNearbyMobV1)==528,"borrowed nearby monster ABI");
static_assert(sizeof(TqtRadarScanV1)==20,"borrowed radar snapshot ABI");
static_assert(sizeof(TqtRadarLootV1)==2052,"owned loot caption ABI");
static_assert(sizeof(TqtRadarInspectorV1)==12,"inspector registration ABI");
#endif
#endif
