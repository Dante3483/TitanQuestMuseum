#ifndef TQM_ADDON_H
#define TQM_ADDON_H
#include <stdint.h>
#include <wchar.h>
#if defined(_WIN32)
#define TQM_CALL __cdecl
#else
#define TQM_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define TQM_ADDON_API_VERSION 1u
#define TQM_FRAME_DRAW_AVAILABLE 1u
#define TQM_FRAME_SOURCES_READY 2u
#define TQM_FRAME_KEYBOARD_BUSY 4u
#define TQM_COPY_OK 0
#define TQM_COPY_STALE 1
#define TQM_COPY_CAPACITY 2
#define TQM_COPY_INVALID 3
#define TQM_COPY_OUTSIDE_FRAME 4
/* POD only. Caller owns all output buffers. No CRT/STL objects cross DLLs.
   Requires AE x86, default MSVC structure packing. Callbacks remain valid until
   process exit; registration pins the callback module. No hot unloading. */
typedef struct TqmRectV1 {float x,y,w,h;} TqmRectV1;
typedef struct TqmColorV1 {float r,g,b,a;} TqmColorV1;
typedef struct TqmFarmTargetV1 {char record[512];char name[1024];} TqmFarmTargetV1;
typedef struct TqmFrameV1 {
    uint32_t size,version,flags,ticks,sourceRevision;
    int32_t difficulty,canvasWidth,canvasHeight;
    const void* localPlayer; /* borrowed; read during this callback only */
    void* renderContext;   /* borrowed; valid only for this frame callback */
} TqmFrameV1;
typedef void (TQM_CALL* TqmOnFrameV1)(void* user,const TqmFrameV1* frame);
typedef int32_t (TQM_CALL* TqmOnWindowMessageV1)(void* user,void* hwnd,uint32_t message,
                                              uintptr_t wparam,intptr_t lparam,uint32_t keyboardBusy);
typedef int32_t (TQM_CALL* TqmOnKeyV1)(void* user,int32_t scanCode,int32_t state,uint32_t keyboardBusy);
typedef struct TqmCallbacksV1 {
    uint32_t size,version;
    void* user;
    TqmOnFrameV1 onFrame;
    TqmOnWindowMessageV1 onWindowMessage;
    TqmOnKeyV1 onKey;
} TqmCallbacksV1;
typedef struct TqmAddonApiV1 {
    uint32_t size,version;
    /* Thread-safe, once per add-on. Returns nonzero registration token, 0 on failure.
       onFrame is mandatory. Window/key callbacks are optional and return 1 to claim input. */
    uint32_t (TQM_CALL* registerCallbacks)(const TqmCallbacksV1* callbacks);
    /* Frame callback only. NULL/0 queries count. Revision is supplied by TqmFrameV1.
       CAPACITY/STALE/INVALID write no rows. Record paths are normalized, names UTF-8. */
    int32_t (TQM_CALL* copyFarmTargets)(uint32_t revision,TqmFarmTargetV1* rows,
                                      uint32_t capacity,uint32_t* count);
    /* Frame callback only; context is frame->renderContext. Text is UTF-16 on Windows.
       alignment: 0 left, 1 center, 2 right. Draw functions allocate no shared objects. */
    void (TQM_CALL* fillRect)(void* context,const TqmRectV1* rect,const TqmColorV1* color);
    float (TQM_CALL* measureText)(void* context,const wchar_t* text,int32_t size);
    void (TQM_CALL* drawText)(void* context,const TqmRectV1* rect,const wchar_t* text,
                             int32_t size,const TqmColorV1* color,int32_t alignment);
} TqmAddonApiV1;
typedef const TqmAddonApiV1* (TQM_CALL* TqmGetAddonApi)(uint32_t requestedVersion);
/* Resolve the exact undecorated export "TQM_GetAddonApi" on TitanQuestCore.asi.
   New consumers resolve TQT_GetCoreApi (tqt_core.h) and use its addons table.
   Older providers or incompatible versions return NULL / lack the export. */
const TqmAddonApiV1* TQM_CALL TQM_GetAddonApi(uint32_t requestedVersion);
#ifdef __cplusplus
}
static_assert(sizeof(TqmFarmTargetV1)==1536,"TQM target row ABI");
#if defined(_M_IX86)
static_assert(sizeof(TqmFrameV1)==40,"TQM x86 frame ABI");
static_assert(sizeof(TqmCallbacksV1)==24,"TQM x86 callbacks ABI");
static_assert(sizeof(TqmAddonApiV1)==28,"TQM x86 API ABI");
#endif
#endif
#endif
