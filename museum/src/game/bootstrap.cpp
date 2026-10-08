// dllmain.cpp - entry point of the Unique Collection Tab mod (built as TitanQuestMuseum.asi).
//
// The file is a plain DLL with the .asi extension Ultimate ASI Loader looks for; the loader
// LoadLibrary's it. Dependent add-ons can discover the versioned TQM_GetAddonApi export.
//
// DllMain stays minimal on purpose: open the log, install the fault watchdog, start one
// worker thread, return. Everything that can block or wait happens on that worker.
//
// THE LOADER DECIDES WHEN THIS RUNS. A winmm.dll loader is mapped through the exe's import
// table at process start; a dinput8.dll loader is mapped when the engine initialises input,
// i.e. after the engine has read its database (TQ AE: the dinput8 hooks were live 3.4 s before
// LoadMainDatabase in the run, but that is the loader's timing, not a promise). Everything
// below therefore has to work both ways: see hookLateLoadTick (hooks.h).

#include <windows.h>

#include "game/hooks.h"
#include "game/core_client.h"
#include "game/collection_bridge.h"
#include "game/journal_import.h"
#include "game/game_api.h"
#include "backend/collection_runtime.h"
#include "game/storage_adapter.h"
#include "game/tooltip_adapter.h"
#include "game/view_adapter.h"
#include "game/bindings.h"
#include "core/configuration.h"
#include "game/catalogue_generation.h"
#include "core/logging.h"
#include "core/paths.h"
#include "core/version.h"

namespace {

// Never hard-coded: integration::utModPathW (ut_paths.h) resolves the mod folder - the TitanQuestMuseum folder
// beside this DLL - and creates it on first use. Filled in DllMain before anything else logs.
wchar_t kLogPath[MAX_PATH] = {0};
wchar_t kIniPath[MAX_PATH] = {0};

HANDLE g_worker = nullptr;
HANDLE g_stopEvent = nullptr;
HMODULE g_selfModule = nullptr;
PVOID g_veh = nullptr;

// ---- the fault watchdog -----------------------------------------------------------------------
// A first-chance access violation with this DLL on the stack is logged AT DEBUG, once per fault,
// with the module+rva, both parameters and the thread - and then handed straight on with
// EXCEPTION_CONTINUE_SEARCH. It NEVER handles anything: the engine's own SEH runs exactly as it
// would have, and a fault the mod itself catches is reported as a warning by the feature that
// caught it, not here.
//   * never for the mod's own guarded reads - they fault by design and count their own faults;
//   * rate-capped, because a game that faults in a loop must not fill the disk;
//   * never on the log thread's own faults (that would recurse through the log lock);
//   * C++ exceptions (0xE06D7363), DBG_PRINTEXCEPTION and breakpoints are ignored - the engine
//     throws C++ exceptions as a matter of course.
volatile LONG g_vehLogged = 0;
volatile LONG g_vehInside = 0;
DWORD g_vehThread = 0;

LONG CALLBACK utVectoredHandler(EXCEPTION_POINTERS* ep) {
    if (!ep || !ep->ExceptionRecord) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != (DWORD)EXCEPTION_ACCESS_VIOLATION) return EXCEPTION_CONTINUE_SEARCH;
    // Nothing below is on a hot path, but a log level of warn or error means the reader asked for
    // less than this: the fault line and the repeat counter are both debug.
    // FIRST, before the guard depth: the level is a plain global, the guard depth is thread-local
    // storage in a DLL the loader mapped late. Both tests must pass to log, so the order does not
    // change what is logged - it keeps the default log level from touching this DLL's TLS slot on
    // every first-chance exception in the process, on threads that existed before it was loaded.
    if (!integration::logWants(integration::UT_LOG_DEBUG)) return EXCEPTION_CONTINUE_SEARCH;
    // The mod's own DELIBERATE, SEH-handled reads fault by design (a guarded read of an engine
    // pointer that turned out stale), and each one is counted where it happens. A vectored
    // handler runs before the __except that owns them, so without this test every one of them
    // printed a fault line and buried the real ones.
    if (integration::guardDepth() > 0) return EXCEPTION_CONTINUE_SEARCH;
    // The ENGINE raises and handles first-chance access violations of its own as a matter of
    // course. They are not the mod's business and would drown the 32-line budget. Log a fault
    // only when this DLL is involved: the faulting address is inside it, or one of its frames is
    // on the faulting thread's stack.
    {
        const unsigned char* base = (const unsigned char*)g_selfModule;
        size_t size = 0;
        if (base) {
            const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
            const IMAGE_NT_HEADERS32* nt = (const IMAGE_NT_HEADERS32*)(base + dos->e_lfanew);
            size = nt->OptionalHeader.SizeOfImage;
        }
        bool ours = false;
        const unsigned char* fault = (const unsigned char*)ep->ExceptionRecord->ExceptionAddress;
        if (base && fault >= base && fault < base + size) ours = true;
        if (!ours && base) {
            void* frames[48];
            const USHORT n = RtlCaptureStackBackTrace(0, 48, frames, nullptr);
            for (USHORT i = 0; i < n && !ours; ++i) {
                const unsigned char* f = (const unsigned char*)frames[i];
                if (f >= base && f < base + size) ours = true;
            }
        }
        if (!ours) return EXCEPTION_CONTINUE_SEARCH;
    }
    const DWORD tid = GetCurrentThreadId();
    if (tid == g_vehThread) return EXCEPTION_CONTINUE_SEARCH;  // re-entered on this thread
    if (InterlockedCompareExchange(&g_vehLogged, 0, 0) >= 32) return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&g_vehInside, 1)) return EXCEPTION_CONTINUE_SEARCH;
    g_vehThread = tid;
    // One line per faulting ADDRESS, then a count. The first line carries everything a reader
    // needs, the rest would only burn the budget.
    {
        static void* seenAddr[8] = {};
        static volatile LONG seenCount[8] = {};
        void* addr = ep->ExceptionRecord->ExceptionAddress;
        int slot = -1;
        for (int i = 0; i < 8; ++i) {
            if (seenAddr[i] == addr) { slot = i; break; }
            if (!seenAddr[i]) { seenAddr[i] = addr; slot = i; break; }
        }
        if (slot >= 0) {
            const LONG n = InterlockedIncrement(&seenCount[slot]);
            if (n != 1 && n != 10 && n != 100 && n != 1000) {
                g_vehThread = 0;
                InterlockedExchange(&g_vehInside, 0);
                return EXCEPTION_CONTINUE_SEARCH;  // counted, not logged
            }
            if (n != 1) integration::logD("watchdog: the fault at %p has now happened %ld times", addr, n);
        }
    }
    InterlockedIncrement(&g_vehLogged);
    integration::logFault("a first-chance access violation (vectored handler)", ep->ExceptionRecord, tid);
    g_vehThread = 0;
    InterlockedExchange(&g_vehInside, 0);
    return EXCEPTION_CONTINUE_SEARCH;  // never swallowed
}

void logIdentity() {
    wchar_t self[MAX_PATH] = {0};
    wchar_t exe[MAX_PATH] = {0};
    GetModuleFileNameW(g_selfModule, self, MAX_PATH);
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    integration::logI("=== Titan Quest Museum %s (Titan Quest AE, x86) ===", UT_VERSION);
    integration::logI("pid=%lu  host exe = \"%S\"", GetCurrentProcessId(), exe);
    integration::logI("mod dll   = \"%S\"", self);
}

// THE LOAD-ORDER AUDIT. Everything below that could have happened before this DLL existed, and
// what covers it:
//   Engine::LoadMainDatabase may already have run, so its detour never fires. Covered by
//     hookLateLoadTick (hooks.cpp): the database checksum is non-zero once the load is over, and
//     the tick then says so. It runs on the game thread from hk_PresentSurface every frame and on
//     this thread once a second, so the frame tick normally wins; either way one interlocked
//     guard lets exactly one of them say it. has no overlay to load.
//   TQ.exe's .text is plain (no DRM stub), so the two exe signatures are scanned here, early.
//   Engine.dll and Game.dll are certainly loaded, so waitForGameModules returns at once.
//   Every other detour is on a function the engine calls per frame, per world or per UI action,
//     never once at start-up, so a later install only means a later first call.
// What a late load does cost: the fault watchdog is armed in DllMain, so a first-chance access
// violation before that is not logged.
DWORD WINAPI workerMain(LPVOID) {
    // The settings file is read FIRST, on this thread rather than under the loader lock, because
    // log_level has to be in force before the export and detour lines are written: a reader who
    // asks for debug is asking for exactly that start-up detail.
    integration::configReload(kIniPath);
    if(!integration::coreConnect(120000)){integration::logE("Museum OFF: TitanQuestCore.asi is missing, incompatible or not ready");return 1;}
    integration::logI("settings file = \"%S\" (re-read once a second)", kIniPath);
    integration::logD("worker thread started");

    if (!integration::waitForGameModules(60000)) {
        integration::logE("the mod is OFF: Engine.dll and Game.dll never loaded");
        return 1;
    }
    if (!integration::resolveExports()) {
        integration::logE("the mod is OFF: this build does not match the game - required exports missing");
        return 2;
    }

    // The generated data files. Runs here, on the worker, before every loader reads them and
    // before any hook exists: a first launch or a game update costs a few seconds, once.
    integration::generateEnsure(g_selfModule);

    // The DECODED and SIGNATURE rows: the offsets out of the getters' own bytes, the vftable
    // slots found by address, the two counted TQ.exe signatures. Must run BEFORE the gate, which
    // treats an early row that never reported as a failure.
    integration::decodeBindings();

    // the collection page's group table (uniq-groups.txt, just generated), packed into
    // 16 x 15 pages of uniform slots. A failure only makes the view unavailable; the mod runs on.
    museum::game::initializeCollection(g_selfModule);
    // the journal (ut_rescue, TQ-1) - the folder only; the save set is chosen at the first world.
    museum::game::importReferenceJournals(g_selfModule);
    integration::storeInit(g_selfModule);

    // THE BINDINGS GATE. Everything the mod knows about the game's memory that is not a name has now
    // reported whether it could still be found and whether it still looks like itself. One INFO
    // line says how many of each class; the whole table goes to the log at debug. If a single
    // EARLY binding failed, nothing at all is installed - no detour, no tab, no deposit - because
    // a mod that is half bound to a game it does not recognise is the one thing that could damage
    // a save.
    if (!integration::bindingsGate(integration::g_tq.resolvedByName, integration::g_tq.missingRequired)) return 4;

    // the collection tooltip line - its six exports and its two prepared lines, before the
    // detours (hooksInstall installs and counts its five). A miss turns the line off, never the mod.
    integration::tooltipInit(g_selfModule);
    if (!integration::hooksInstall()) {
        integration::logE("the mod is OFF: no detour could be installed");
        return 3;
    }
    // G8: the view's own gate - one line, "view: available" or "view: unavailable - why".
    integration::viewInit(integration::hooksViewOk());
    integration::logI("READY: exports resolved, detours installed | %s", integration::tooltipStatus());
    integration::logFlush();  // the banner's last line is on disk before the first frame

    DWORD lastHeartbeat = GetTickCount();
    // The stall check: frames-not-advancing. A "freeze" is usually not a deadlock at all - the
    // game has CRASHED and a crash reporter is waiting on a dialog nobody can see behind the
    // fullscreen window. That is one OpenEvent away from being said out loud in the log.
    unsigned long long lastFrames = integration::hookFrameCount();
    DWORD lastFrameMove = GetTickCount();
    bool stallReported = false;
    // The worker is what does the file I/O. It wakes on its own stop event or once a second - and
    // every wake flushes the buffered log.
    // the journal's event wakes the worker for a lazy write (a pending mark settled).
    HANDLE waits[2] = {g_stopEvent, integration::journalEvent()};
    const DWORD waitCount = waits[1] ? 2 : 1;
    for (;;) {
        const DWORD w = WaitForMultipleObjects(waitCount, waits, FALSE, 1000);
        if (w == WAIT_OBJECT_0) break;
        integration::configReload(kIniPath);

        integration::hookLateLoadTick(false);
        integration::hookSayCounts();
        integration::storeWorkerTick();
        integration::logFlush();

        const DWORD now = GetTickCount();
        // ---- the stall check -------------------------------------------------------------
        {
            const unsigned long long frames = integration::hookFrameCount();
            if (frames != lastFrames) {
                lastFrames = frames;
                lastFrameMove = now;
                if (stallReported) {
                    integration::logI("stall over: frames are advancing again (frames=%llu)", frames);
                    stallReported = false;
                }
            } else if (integration::generateBusy() || frames == 0) {
                // GD's branch: a generation holds no frame back, but it is not a crashed game
                // either. TQ: and the check is armed only once the first frame has been
                // presented - before it, the engine is still loading (the smoke saw the
                // stall line fire at +5 s with frames=0, a start-up load, not a stall).
                lastFrameMove = now;
            } else if (!stallReported && now - lastFrameMove >= 5000) {
                stallReported = true;
                HANDLE ev = OpenEventW(SYNCHRONIZE, FALSE, L"CRASHREPORT");
                const bool crashDll = GetModuleHandleW(L"CrashReport.dll") != nullptr;
                integration::logW("stall: %lu s with no new frame - %s", (now - lastFrameMove) / 1000,
                         (ev || crashDll) ? "the game has CRASHED, not deadlocked"
                                          : "no crash reporter is up: a real stall or a long load");
                integration::logD("stall: frames=%llu updates=%llu, CRASHREPORT event %s, "
                         "CrashReport.dll %s",
                         frames, integration::hookUpdateCount(), ev ? "EXISTS" : "absent",
                         crashDll ? "loaded" : "not loaded");
                if (ev) CloseHandle(ev);
                integration::logFlush();
            }
        }
        // The heartbeat is trace, and its status string is built only when trace is on.
        if (now - lastHeartbeat >= 10000) {
            lastHeartbeat = now;
            if (integration::logWants(integration::UT_LOG_TRACE)) {
                integration::logT("heartbeat: frames=%llu updates=%llu caravanOpen=%d opens=%ld | %s | %s",
                         integration::hookFrameCount(), integration::hookUpdateCount(),
                         integration::hookCaravanOpen() ? 1 : 0, integration::hookCaravanOpens(),
                         integration::viewStatus(), integration::bindingsSummary());
            }
        }
    }
    integration::logD("worker thread stopping");
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            g_selfModule = module;
            DisableThreadLibraryCalls(module);
            integration::utModPathW(module, L"TitanQuestMuseum.log", kLogPath, MAX_PATH);
            integration::utModPathW(module, L"TitanQuestMuseum.ini", kIniPath, MAX_PATH);
            integration::logInit(kLogPath);
            logIdentity();
            // The mod folder is resolved before the log file exists, so it cannot report itself.
            if (const char* warn = integration::utModDirWarning()) integration::logW("%s", warn);

            // The fault watchdog: FIRST in the chain, so a first-chance access
            // violation is logged before anything else can consume it. It only ever logs.
            g_veh = AddVectoredExceptionHandler(1, utVectoredHandler);
            if (g_veh) {
                integration::logD("watchdog: a first-chance access violation with this mod on the stack is "
                         "logged (module+rva, parameters, thread) and handed on unchanged");
            } else {
                integration::logW("watchdog: the fault handler could NOT be installed - a crash will leave "
                         "nothing in this log");
            }

            g_stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            g_worker = CreateThread(nullptr, 0, workerMain, nullptr, 0, nullptr);
            if (!g_worker) {
                integration::logE("FATAL: CreateThread for the worker failed, err=%lu", GetLastError());
            }
            break;
        }
        case DLL_PROCESS_DETACH: {
            // NOTHING that touches another thread happens here.
            // MH_DisableHook/MH_Uninitialize suspend every thread in the process, which under
            // the loader lock is a textbook DllMain deadlock, and WaitForSingleObject on the
            // worker can time out and then unmap the image while a detour is still live. The
            // hooks are left in place until process exit; the worker is asked to stop but is
            // never waited on. (The loader never calls FreeLibrary on an .asi, so an unload
            // before process exit does not happen in practice.)
            // the page member must not keep the mod sack past this DLL (a guarded write;
            // nothing freed, no engine call under the loader lock).
            integration::viewDetach();
            integration::logI("shutting down: frames=%llu updates=%llu (processExit=%d)",
                     integration::hookFrameCount(), integration::hookUpdateCount(), reserved ? 1 : 0);
            integration::logD("the detours are deliberately left installed: disabling them here would "
                     "suspend every thread under the loader lock");
            if (!reserved && g_stopEvent) SetEvent(g_stopEvent);
            integration::logI("=== Titan Quest Museum unloaded ===");
            integration::logShutdown();
            break;
        }
        default:
            break;
    }
    return TRUE;
}
