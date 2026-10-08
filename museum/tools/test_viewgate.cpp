// test_viewgate.cpp - the collection view's state machine (src\game/view_safety.h), offline.
// NO GAME, NO FILE, NO WINDOWS. The header is included, so this runs exactly the code the mod runs.
//
// A simulated Transfer page: the cached member (REAL or MOD), the prototypes, the engine's tab.
// Every event order up to kDepth events over the alphabet below is enumerated (a DFS over the
// state, so every prefix is a case of its own), and after EVERY step:
//   I1  view OFF  -> the member is the real sack and no prototype exists
//   I2  view ON   -> caravan open, Transfer tab, world up, not MP, no fault, bindings complete
//   I3  every SAVE (the three StreamOut calls after CaravanGoodbye) sees the real sack - with the
//       StreamOut assert (G3) DISABLED, so G2 alone must hold the door
//   I4  every LOAD (the open) sees the real sack
//   I5  once faulted, no toggle ever turns the view on again
//   I6  the getter's other callers (quick-move) never get the mod sack
// Then a MUTANT run with G2's goodbye re-point removed must be caught by I3 and, with G3 enabled,
// rescued at the save - proof the harness can see the failure it guards against.
//
// the owned marks' grid-origin check (src\game/grid_validation.h) - a measured 1366 x 768 window, the
// records-consistent case, each refusal, and every hover sequence up to depth 5 over 9 hover kinds
// x 4 bound sets against an independent oracle: an ACCEPTED origin always passed check 1 on every
// counted hover, came from two different prototypes with the same origin, lies inside the canvas,
// and is inside the records' window rect or was vouched for by the cursor on two hovers.

#include <stdio.h>
#include <string.h>

#include <limits>

#include "../src/game/grid_validation.h"
#include "../src/game/caravan_geometry.h"
#include "../src/game/slot_art.h"
#include "../src/backend/silhouette_names.h"   // the gray icons (pure)
#include "../src/backend/deposit_rules.h"
#include "../src/game/view_safety.h"
#include "../src/backend/window_math.h"     // the per-tick clamp (GD's utClampRow)
#include "../src/game/cost_probe.h"   // the cost meter

namespace {

int g_fail = 0;
long long g_checks = 0;

void ok(bool cond, const char* what, const char* detail) {
    printf("[viewgate]  %s  %-52s %s\n", cond ? "PASS" : "FAIL", what, detail ? detail : "");
    if (!cond) ++g_fail;
}

enum Act {
    aToggle, aFrame, aTab0, aTab1, aTab2, aClose, aOpen, aWorldUp, aWorldDown, aMpOn, aMpOff,
    aFault, aBindings, aCount
};
const char* const kActName[aCount] = {"toggle", "frame", "tab0", "tab1", "tab2", "close", "open",
                                      "worldUp", "worldDown", "mpOn", "mpOff", "fault",
                                      "bindingsIncomplete"};

struct Sim {
    integration::UtViewState vs;
    int member = 0;        // 0 = the real Transfer sack, 1 = the mod sack
    int protos = 0;        // prototypes alive in the mod sack
    int engineTab = -1;    // what the caravan window really shows
    bool engineOpen = false;
    bool faultedEver = false;
    int g3Fired = 0;
};

struct Opts {
    bool g2Goodbye = true;  // the CaravanGoodbye pre-detour re-points (G2)
    bool g3 = false;        // the StreamOut assert (G3)
};

// What viewForceOff does: re-point, then destroy.
void forceOff(Sim* s) {
    s->member = 0;
    s->protos = 0;
}

void applyStep(Sim* s, const integration::UtViewStep& st) {
    if (st.turnedOn) s->protos = 12;
    if (st.turnedOff) forceOff(s);
}

// ---- the take-path model of InventorySack::GetItemUnderPoint --------
// What the caller does with the id it gets. The right-click clones it into the inventory, the held
// pick-up removes it with InventorySack::RemoveItem, the page mouse handler branches on its two
// bools: b1 = pick-up (SetId + RemoveItemFrom*), else b2 = clone (0xC0530), else the HOVER hands
// it to the tooltip only. A take of a PROTOTYPE id is the door's breach; a hover over a prototype
// that gets 0 is the earlier bug (no tooltip).
enum { kIdNone = 0, kIdProto = 1, kIdReal = 2 };
struct UnderOutcome {
    bool takeOfProto;   // a prototype id reached a take branch
    bool tooltip;       // the tooltip branch saw the prototype id
};
typedef bool (*UnderVerdict)(int site, bool modSack, int frame);
UnderOutcome underModel(UnderVerdict verdict, int site, bool modSack, bool verified, int b1,
                        int b2) {
    const int frame = integration::utUnderFrameKind(verified, (unsigned char)b1, (unsigned char)b2);
    const bool refused = verdict(site, modSack, frame);
    const int got = refused ? kIdNone : (modSack ? kIdProto : kIdReal);
    UnderOutcome o = {false, false};
    // The engine branches on the LOW BYTE being non-zero (`cmp byte ptr [ebp+8],0`).
    const bool press = b1 != 0, modified = b2 != 0;
    switch (site) {
    case integration::kUtUnderRightClick:
    case integration::kUtUnderHeldPickup:
        o.takeOfProto = got == kIdProto;
        break;
    case integration::kUtUnderMouseHandler:
        if (press || modified) {
            o.takeOfProto = got == kIdProto;
        } else {
            o.tooltip = got == kIdProto;
        }
        break;
    default:
        break;
    }
    return o;
}
bool verdictRefuseAll(int site, bool modSack, int) {   // the earlier verdict: every site refused
    return modSack && site != integration::kUtUnderOther;
}
bool verdictNoBools(int site, bool modSack, int) {   // a MUTANT: the handler never refused
    return modSack && (site == integration::kUtUnderRightClick || site == integration::kUtUnderHeldPickup);
}

// Returns false (and fills `why`) when an invariant broke during the step.
bool step(Sim* s, int a, const Opts& o, char* why, size_t cap) {
    ++g_checks;
    switch (a) {
    case aToggle:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvToggle, 0));
        if (s->faultedEver && s->vs.on) {
            _snprintf_s(why, cap, _TRUNCATE, "I5: ON after a fault");
            return false;
        }
        break;
    case aFrame:
        // The caravan window's per-frame update: SetCaravanMode(current tab), then the active
        // page's accessor, which caches whatever the getter returns.
        if (s->engineOpen) {
            applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvModeChanged, s->engineTab));
            if (s->engineTab == 1) {
                const bool sub = integration::utViewSubstitute(true, s->vs.on, s->vs.mode, s->vs.world,
                                                      s->vs.mpUnknown);
                if (sub && s->protos == 0) {
                    _snprintf_s(why, cap, _TRUNCATE, "substituted an EMPTY mod sack");
                    return false;
                }
                s->member = sub ? 1 : 0;
            }
        }
        // I6: the quick-move path calls the same getter from elsewhere.
        if (integration::utViewSubstitute(false, s->vs.on, s->vs.mode, s->vs.world, s->vs.mpUnknown)) {
            _snprintf_s(why, cap, _TRUNCATE, "I6: a non-accessor caller got the mod sack");
            return false;
        }
        break;
    case aTab0:
    case aTab1:
    case aTab2:
        if (s->engineOpen) {
            s->engineTab = a - aTab0;
            applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvModeChanged, s->engineTab));
        }
        break;
    case aClose:
        if (s->engineOpen) {
            // CaravanGoodbye PRE-detour, then the three saves 8-25 ms later, NO frame between.
            const integration::UtViewStep st = integration::utViewApply(&s->vs, integration::kUtViewEvGoodbye, 0);
            if (o.g2Goodbye) applyStep(s, st);
            for (int page = 0; page < 3; ++page) {
                if (o.g3 && s->member == 1) {   // StreamOut PRE: re-point, latch, force OFF
                    s->member = 0;
                    ++s->g3Fired;
                    applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvStreamOutModSack, 0));
                    forceOff(s);
                    s->faultedEver = true;
                }
                if (s->member != 0) {
                    _snprintf_s(why, cap, _TRUNCATE, "I3: a save streamed the MOD sack");
                    return false;
                }
            }
            s->engineOpen = false;
        }
        break;
    case aOpen:
        if (!s->engineOpen) {
            s->engineOpen = true;
            s->engineTab = 0;   // TQ opens on whatever tab; the next frame sets the mode
            applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvOpen, 0));
            if (s->member != 0) {
                _snprintf_s(why, cap, _TRUNCATE, "I4: the load ran into the MOD sack");
                return false;
            }
        }
        break;
    case aWorldUp:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvWorldUp, 0));
        break;
    case aWorldDown:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvWorldDown, 0));
        break;
    case aMpOn:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvMpChanged, 1));
        break;
    case aMpOff:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvMpChanged, 0));
        break;
    case aFault:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvFault, 0));
        s->faultedEver = true;
        break;
    case aBindings:
        applyStep(s, integration::utViewApply(&s->vs, integration::kUtViewEvBindingsIncomplete, 0));
        break;
    }
    // I1 / I2 after every step (the G2 mutant breaks I1 by construction: only I3 is asked of it).
    if (o.g2Goodbye && !s->vs.on && (s->member != 0 || s->protos != 0)) {
        _snprintf_s(why, cap, _TRUNCATE, "I1: OFF but member=%d protos=%d", s->member, s->protos);
        return false;
    }
    if (s->vs.on && !(s->vs.open && s->vs.mode == 1 && s->vs.world && !s->vs.mpUnknown &&
                      !s->vs.faulted && s->vs.bindingsOk)) {
        _snprintf_s(why, cap, _TRUNCATE, "I2: ON outside its conditions");
        return false;
    }
    return true;
}

struct Walk {
    Opts opts;
    int depth;
    long long cases = 0;
    long long onReached = 0;       // sequences in which the view was ever ON
    long long savedWhileSub = 0;   // closes that happened while the member held the mod sack
    long long g3Fired = 0;
    int failures = 0;
    char firstFail[512] = {0};
};

void dfs(Walk* w, const Sim& s, int* seq, int n) {
    if (n == w->depth) return;
    for (int a = 0; a < aCount; ++a) {
        Sim t = s;
        seq[n] = a;
        const bool subBefore = t.member == 1;
        char why[160] = {0};
        const bool good = step(&t, a, w->opts, why, sizeof(why));
        ++w->cases;
        if (t.vs.on) ++w->onReached;
        if (a == aClose && subBefore) ++w->savedWhileSub;
        w->g3Fired += t.g3Fired - s.g3Fired;
        if (!good) {
            if (!w->failures++) {
                int k = _snprintf_s(w->firstFail, sizeof(w->firstFail), _TRUNCATE, "%s after:", why);
                for (int i = 0; i <= n && k > 0 && k < (int)sizeof(w->firstFail) - 24; ++i)
                    k += _snprintf_s(w->firstFail + k, sizeof(w->firstFail) - k, _TRUNCATE, " %s",
                                     kActName[seq[i]]);
            }
            continue;   // do not extend a broken sequence
        }
        dfs(w, t, seq, n + 1);
    }
}

Sim fresh(bool bindingsOk) {
    Sim s;
    s.vs.bindingsOk = bindingsOk;
    return s;
}

// Run one explicit sequence; returns the final Sim and whether every step held.
bool run(const int* acts, int n, const Opts& o, Sim* out, char* why, size_t cap) {
    Sim s = fresh(true);
    for (int i = 0; i < n; ++i) {
        if (!step(&s, acts[i], o, why, cap)) {
            if (out) *out = s;
            return false;
        }
    }
    if (out) *out = s;
    return true;
}

}  // namespace

int main() {
    char d[512];

    // ---- the pure predicates ---------------------------------------------------------------
    // which sack the substituting getter hands the page
    ok(integration::utViewGetterPicksMod(true, 5, false) && integration::utViewGetterPicksMod(true, 5, true) &&
           integration::utViewGetterPicksMod(true, 0, true) && !integration::utViewGetterPicksMod(true, 0, false) &&
           !integration::utViewGetterPicksMod(false, 5, false) && !integration::utViewGetterPicksMod(false, 0, true) &&
           !integration::utViewGetterPicksMod(true, -1, true),
       "getter: mod sack with prototypes or an empty OWN page; a failed build -> real", "7 rows");
    ok(integration::utViewSubstitute(true, true, 1, true, false) &&
           !integration::utViewSubstitute(false, true, 1, true, false) &&
           !integration::utViewSubstitute(true, false, 1, true, false) &&
           !integration::utViewSubstitute(true, true, 0, true, false) &&
           !integration::utViewSubstitute(true, true, 2, true, false) &&
           !integration::utViewSubstitute(true, true, 1, false, false) &&
           !integration::utViewSubstitute(true, true, 1, true, true),
       "G1 substitute(accessor, on, mode 1, world, state known) only (MP substitutes)",
       "7 combinations");
    {
        integration::UtViewState s;
        int refusals[6];
        refusals[0] = integration::utViewRefusal(s);
        s.bindingsOk = true;
        s.faulted = true;
        refusals[1] = integration::utViewRefusal(s);
        s.faulted = false;
        refusals[2] = integration::utViewRefusal(s);
        s.world = true;
        s.mpUnknown = true;   // only an UNKNOWN session state refuses, not multiplayer
        refusals[3] = integration::utViewRefusal(s);
        s.mpUnknown = false;
        s.mp = true;
        refusals[4] = integration::utViewRefusal(s);
        s.open = true;
        s.mode = 2;
        refusals[5] = integration::utViewRefusal(s);
        s.mode = 1;
        const int allowed = integration::utViewRefusal(s);
        ok(refusals[0] == integration::kUtViewWhyRefusedBindings &&
               refusals[1] == integration::kUtViewWhyRefusedFaulted &&
               refusals[2] == integration::kUtViewWhyRefusedNoWorld &&
               refusals[3] == integration::kUtViewWhyRefusedMp &&
               refusals[4] == integration::kUtViewWhyRefusedClosed &&
               refusals[5] == integration::kUtViewWhyRefusedNotTransfer && allowed == integration::kUtViewWhyOk,
           "G8 every refusal names itself, in order", integration::utViewWhyText(refusals[5]));
    }
    {
        // every event that must force OFF does, from a clean ON state
        const int evs[] = {integration::kUtViewEvToggle,   integration::kUtViewEvOpen,      integration::kUtViewEvGoodbye,
                           integration::kUtViewEvWorldUp,  integration::kUtViewEvWorldDown, integration::kUtViewEvFault,
                           integration::kUtViewEvBindingsIncomplete, integration::kUtViewEvStreamOutModSack};
        int offs = 0;
        for (int e : evs) {
            integration::UtViewState s;
            s.bindingsOk = s.world = s.open = true;
            s.mode = 1;
            integration::utViewApply(&s, integration::kUtViewEvToggle, 0);
            const integration::UtViewStep st = integration::utViewApply(&s, e, 0);
            if (st.turnedOff && !s.on) ++offs;
        }
        integration::UtViewState s;
        s.bindingsOk = s.world = s.open = true;
        s.mode = 1;
        integration::utViewApply(&s, integration::kUtViewEvToggle, 0);
        const bool tab = integration::utViewApply(&s, integration::kUtViewEvModeChanged, 0).turnedOff;
        integration::utViewApply(&s, integration::kUtViewEvModeChanged, 1);
        integration::utViewApply(&s, integration::kUtViewEvToggle, 0);
        const bool same = !integration::utViewApply(&s, integration::kUtViewEvModeChanged, 1).turnedOff && s.on;
        const bool mp = integration::utViewApply(&s, integration::kUtViewEvMpChanged, 1).turnedOff;
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%d of 8 events + tab change + MP; mode 1 again keeps it",
                    offs);
        ok(offs == 8 && tab && same && mp, "every OFF trigger turns the view OFF", d);
    }

    // ---- GetItemUnderPoint - every site x sack x frame x bool byte ----------------------
    {
        int cases = 0, breaches = 0, tooltips = 0, missedTooltips = 0, vanillaTouched = 0;
        for (int site = 0; site < 4; ++site)
            for (int mod = 0; mod < 2; ++mod)
                for (int ver = 0; ver < 2; ++ver)
                    for (int b1 = 0; b1 < 256; ++b1)
                        for (int b2 = 0; b2 < 256; ++b2) {
                            ++cases;
                            ++g_checks;
                            const UnderOutcome o =
                                underModel(integration::utUnderPointRefuse, site, mod != 0, ver != 0, b1, b2);
                            if (o.takeOfProto) ++breaches;
                            if (o.tooltip) ++tooltips;
                            const bool provenHover = site == integration::kUtUnderMouseHandler && mod &&
                                                     ver && b1 == 0 && b2 == 0;
                            if (provenHover && !o.tooltip) ++missedTooltips;
                            if (!mod && integration::utUnderPointRefuse(site, false, integration::kUtFrameClick))
                                ++vanillaTouched;
                        }
        _snprintf_s(d, sizeof(d), _TRUNCATE,
                    "%d cases: %d prototype takes, tooltip on %d (the proven hovers), %d missed, "
                    "%d real-sack refusals", cases, breaches, tooltips, missedTooltips,
                    vanillaTouched);
        ok(breaches == 0 && tooltips == 1 && missedTooltips == 0 && vanillaTouched == 0,
           "under-point: no take of a prototype, the proven hover gets its id", d);
        // the two wrong verdicts are CAUGHT: the earlier loses the tooltip, the mutant leaks a take
        int refuseAllTips = 0, mutantTakes = 0;
        for (int b1 = 0; b1 < 2; ++b1)
            for (int b2 = 0; b2 < 2; ++b2) {
                refuseAllTips += underModel(verdictRefuseAll, integration::kUtUnderMouseHandler, true, true, b1, b2).tooltip;
                mutantTakes +=
                    underModel(verdictNoBools, integration::kUtUnderMouseHandler, true, true, b1, b2).takeOfProto;
            }
        _snprintf_s(d, sizeof(d), _TRUNCATE, "verdict: %d tooltip(s); bool-blind mutant: %d take(s)",
                    refuseAllTips, mutantTakes);
        ok(refuseAllTips == 0 && mutantTakes == 3, "under-point: the earlier bug and a bool-blind mutant are caught", d);
    }
    {
        // the frame shape: EBP - return slot == dist + (EBP & 7), EBP 4-aligned
        const unsigned dist = 0x68;
        const bool a = integration::utUnderFrameShape(0x0019F000u, 0x0019F000u - 0x68, dist);        // aligned
        const bool b = integration::utUnderFrameShape(0x0019F004u, 0x0019F004u - 0x6C, dist);        // +4
        const bool c = !integration::utUnderFrameShape(0x0019F004u, 0x0019F004u - 0x68, dist);       // off by 4
        const bool e = !integration::utUnderFrameShape(0x0019F002u, 0x0019F002u - 0x6A, dist);       // misaligned
        const bool f = !integration::utUnderFrameShape(0x0019F000u, 0x0019F000u - 0x68, 0);          // unknown
        const bool g = !integration::utUnderFrameShape(0x0019F000u, 0x0019F000u + 0x68, dist);       // below
        g_checks += 6;
        ok(a && b && c && e && f && g, "under-point: the frame shape is exact",
           "aligned, +4, off by 4, misaligned, dist 0, EBP below the slot");
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const bool m1 = integration::utUnderPointMatches(100.0f, 40.0f, 150.0f, 250.0f, 13.0f, 2.0f, 37.0f, 208.0f);
        const bool m2 = !integration::utUnderPointMatches(101.0f, 40.0f, 150.0f, 250.0f, 13.0f, 2.0f, 37.0f, 208.0f);
        const bool m3 = !integration::utUnderPointMatches(nan, 40.0f, 150.0f, 250.0f, 13.0f, 2.0f, 37.0f, 208.0f);
        const bool m4 = integration::utUnderFrameKind(true, 0, 0) == integration::kUtFrameHover &&
                        integration::utUnderFrameKind(true, 1, 0) == integration::kUtFrameClick &&
                        integration::utUnderFrameKind(true, 0, 1) == integration::kUtFrameClick &&
                        integration::utUnderFrameKind(true, 2, 0) == integration::kUtFrameUnknown &&
                        integration::utUnderFrameKind(false, 0, 0) == integration::kUtFrameUnknown;
        g_checks += 4;
        ok(m1 && m2 && m3 && m4, "under-point: the point cross-check and the bool bytes",
           "exact passes, 1 px off fails, NaN fails; 0/0 hover, 1 click, 2 or unverified unknown");
    }

    // ---- named sequences -------------------------------------------------------------------
    {
        const Opts o;   // G2 on, G3 off: the door must hold on G2 alone
        char why[160] = {0};
        Sim s;
        const int noFrame[] = {aWorldUp, aOpen, aFrame, aTab1, aToggle, aClose};
        bool good = run(noFrame, 6, o, &s, why, sizeof(why));
        ok(good && s.member == 0 && !s.vs.on, "ON then close with NO frame", why);
        const int withFrame[] = {aWorldUp, aOpen, aTab1, aToggle, aFrame, aClose};
        good = run(withFrame, 6, o, &s, why, sizeof(why));
        ok(good && s.member == 0 && s.protos == 0, "ON, a frame (member = mod), close", why);
        const int tabFast[] = {aWorldUp, aOpen, aTab1, aToggle, aFrame, aTab0, aClose};
        good = run(tabFast, 7, o, &s, why, sizeof(why));
        ok(good && s.member == 0, "ON, a frame, tab click, close with no frame", why);
        const int worldDown[] = {aWorldUp, aOpen, aTab1, aToggle, aFrame, aWorldDown, aClose};
        good = run(worldDown, 7, o, &s, why, sizeof(why));
        ok(good && s.member == 0, "ON, a frame, the world unloads, close", why);
        const int mp[] = {aWorldUp, aOpen, aTab1, aToggle, aFrame, aMpOn, aFrame, aClose};
        good = run(mp, 8, o, &s, why, sizeof(why));
        ok(good && s.member == 0 && !s.vs.on, "ON, a frame, multiplayer, close", why);
        const int fault[] = {aWorldUp, aOpen, aTab1, aToggle, aFrame, aFault, aToggle, aFrame};
        good = run(fault, 8, o, &s, why, sizeof(why));
        ok(good && !s.vs.on && s.member == 0, "a fault latches: the next toggle is refused", why);
        const int again[] = {aWorldUp, aOpen, aTab1, aToggle, aFrame, aTab0, aFrame, aTab1,
                             aFrame, aToggle, aFrame, aClose};
        good = run(again, 12, o, &s, why, sizeof(why));
        ok(good && s.member == 0, "ON, Stash and back (OFF), ON again, close", why);
    }

    // ---- the enumeration -------------------------------------------------------------------
    const int kDepth = 7;
    int seq[16];
    {
        Walk w;
        w.opts.g2Goodbye = true;
        w.opts.g3 = false;
        w.depth = kDepth;
        dfs(&w, fresh(true), seq, 0);
        _snprintf_s(d, sizeof(d), _TRUNCATE,
                    "%lld sequences (depth %d, %d events), ON reached in %lld, %lld closes with "
                    "the mod sack cached; %s",
                    w.cases, kDepth, (int)aCount, w.onReached, w.savedWhileSub,
                    w.failures ? w.firstFail : "no invariant broke");
        ok(w.failures == 0 && w.onReached > 0 && w.savedWhileSub > 0,
           "EVERY order: G2 alone keeps the member real at every save", d);
    }
    {
        Walk w;
        w.opts.g2Goodbye = true;
        w.opts.g3 = true;
        w.depth = kDepth;
        dfs(&w, fresh(true), seq, 0);
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%lld sequences, G3 fired %lld times (expected 0)",
                    w.cases, w.g3Fired);
        ok(w.failures == 0 && w.g3Fired == 0, "with G3 on as well: G3 never has to fire", d);
    }
    {
        Walk w;
        w.opts.g2Goodbye = true;
        w.depth = 6;
        dfs(&w, fresh(false), seq, 0);
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%lld sequences, ON reached %lld times", w.cases,
                    w.onReached);
        ok(w.failures == 0 && w.onReached == 0, "bindings incomplete: the view is never ON", d);
    }
    {
        // THE MUTANT: no re-point at CaravanGoodbye. I3 must catch it...
        Walk w;
        w.opts.g2Goodbye = false;
        w.opts.g3 = false;
        w.depth = 6;
        dfs(&w, fresh(true), seq, 0);
        ok(w.failures > 0 && strstr(w.firstFail, "I3:") == w.firstFail,
           "mutant without G2 is CAUGHT by I3", w.firstFail);
        // ... and G3 alone must rescue every save of it.
        Walk w3;
        w3.opts.g2Goodbye = false;
        w3.opts.g3 = true;
        w3.depth = 6;
        dfs(&w3, fresh(true), seq, 0);
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%lld sequences, G3 fired %lld times; %s", w3.cases,
                    w3.g3Fired, w3.failures ? w3.firstFail : "every save saw the real sack");
        ok(w3.failures == 0 && w3.g3Fired > 0, "mutant without G2: G3 alone holds the door", d);
    }

    // ---- the owned marks' grid-origin check (game/grid_validation.h) ---------------------------
    {
        using namespace integration;
        // a 1366 x 768 canvas at UI scale 1, the window's record (10, 65.5) 565 x 637; the
        // engine puts the grid origin at (93, 177) with 32 px cells
        const UtGridBounds win1366 = {1366.0f, 768.0f, 10.0f, 65.5f, 565.0f, 637.0f};
        auto hover = [](float gx, float gy, float x, float y, unsigned id, int col, int row, int w,
                        int h, bool cur, float curDx) {
            UtGridHover v = {};
            v.gridX = gx;
            v.gridY = gy;
            v.x = x;
            v.y = y;
            v.id = id;
            v.col = col;
            v.row = row;
            v.w = w;
            v.h = h;
            v.cw = v.ch = 32;
            v.cursorRead = cur;
            v.cursorX = gx + x + curDx;
            v.cursorY = gy + y;
            return v;
        };
        {   // the measured numbers, the cursor agreeing: accepted although the records disagree
            UtGridState st;
            utGridReset(&st);
            const int r1 = utGridStep(&st, hover(93, 177, 506, 28, 60937, 14, 0, 2, 2, true, 0.0f), win1366);
            const int r2 = utGridStep(&st, hover(93, 177, 10, 10, 60938, 0, 0, 2, 2, true, 1.5f), win1366);
            _snprintf_s(d, sizeof(d), _TRUNCATE, "steps %d, %d; verdict %d at (%.0f,%.0f), window %d",
                        r1, r2, st.verdict, st.gridX, st.gridY, st.windowOk ? 1 : 0);
            ok(r1 == kUtGridFirst && r2 == kUtGridAccepted && st.verdict == 1 && st.gridX == 93.0f &&
                   st.gridY == 177.0f && !st.windowOk,
               "grid: the measured numbers + the cursor -> accepted", d);
        }
        {   // the same numbers without a cursor: waits, and never refuses
            UtGridState st;
            utGridReset(&st);
            utGridStep(&st, hover(93, 177, 506, 28, 60937, 14, 0, 2, 2, false, 0.0f), win1366);
            int waits = 0, last = 0;
            for (int i = 0; i < 1000 && st.verdict == 0; ++i) {
                last = utGridStep(&st, hover(93, 177, 10, 10, 60938, 0, 0, 2, 2, false, 0.0f), win1366);
                if (last == kUtGridWaitCursor) ++waits;
            }
            _snprintf_s(d, sizeof(d), _TRUNCATE, "%d waits (state %d), then %d, verdict %d", waits,
                        st.waits, last, st.verdict);
            ok(waits == 1000 && st.waits == 1000 && last == kUtGridWaitCursor && st.verdict == 0,
               "grid: outside the window, no cursor -> waits, never refused", d);
        }
        {   // the cursor 56 px off (the engine's units are not canvas px): never accepted
            UtGridState st;
            utGridReset(&st);
            for (int i = 0; i < 100 && st.verdict == 0; ++i)
                utGridStep(&st, hover(93, 177, 10.0f + (float)(i % 2) * 64.0f, 10, 60937 + (i % 2),
                                      (i % 2) * 2, 0, 2, 2, true, 56.0f), win1366);
            ok(st.verdict == 0 && st.cursorAgree == 0,
               "grid: a cursor 56 px off never vouches (and never refuses)", nullptr);
        }
        {   // an origin the records agree with: accepted on the second prototype, no cursor needed
            UtGridState st;
            utGridReset(&st);
            utGridStep(&st, hover(37, 192, 40, 40, 7, 1, 1, 1, 1, false, 0.0f), win1366);
            const int r = utGridStep(&st, hover(37, 192, 100, 40, 8, 3, 1, 1, 1, false, 0.0f), win1366);
            ok(r == kUtGridAccepted && st.windowOk, "grid: inside the records' window -> accepted",
               nullptr);
        }
        {   // each refusal
            UtGridState st;
            utGridReset(&st);
            int r = utGridStep(&st, hover(93, 177, 506, 28, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            const int r2 = utGridStep(&st, hover(93, 177, 10, 10, 2, 0, 0, 2, 2, true, 0.0f), win1366);
            ok(r == kUtGridRefusedCell && r2 == kUtGridIgnored && st.verdict == -1,
               "grid: check 1 failing refuses, for good", nullptr);
            utGridReset(&st);
            utGridStep(&st, hover(93, 177, 10, 10, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            r = utGridStep(&st, hover(94, 177, 10, 10, 2, 0, 0, 2, 2, true, 0.0f), win1366);
            ok(r == kUtGridRefusedOrigin && st.verdict == -1,
               "grid: two hovers 1 px apart refuse", nullptr);
            utGridReset(&st);
            utGridStep(&st, hover(93, 177, 10, 10, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            r = utGridStep(&st, hover(93, 177, 20, 20, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            ok(r == kUtGridSamePrototype && st.verdict == 0,
               "grid: the same prototype twice decides nothing", nullptr);
            utGridReset(&st);
            const UtGridBounds small = {600.0f, 600.0f, 0.0f, 0.0f, 600.0f, 600.0f};
            utGridStep(&st, hover(93, 177, 10, 10, 1, 0, 0, 2, 2, true, 0.0f), small);
            r = utGridStep(&st, hover(93, 177, 80, 10, 2, 2, 0, 2, 2, true, 0.0f), small);
            ok(r == kUtGridRefusedCanvas && st.verdict == -1, "grid: off the canvas refuses",
               nullptr);
            utGridReset(&st);
            const float nan = std::numeric_limits<float>::quiet_NaN();
            utGridStep(&st, hover(nan, 177, 10, 10, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            r = utGridStep(&st, hover(nan, 177, 80, 10, 2, 2, 0, 2, 2, true, 0.0f), win1366);
            ok(r == kUtGridRefusedOrigin && st.verdict == -1, "grid: a NaN origin refuses", nullptr);
        }
        {   // 40 decisive hovers with the cursor 10 px behind, then two at rest
            UtGridState st;   // 15 cells apart -> accepted (the wait never refuses)
            utGridReset(&st);
            utGridStep(&st, hover(93, 177, 506, 28, 1, 14, 0, 2, 2, true, 10.0f), win1366);
            int waits = 0;
            for (int i = 0; i < 40; ++i)
                if (utGridStep(&st, hover(93, 177, 10, 10, 2, 0, 0, 2, 2, true, 10.0f), win1366) ==
                    kUtGridWaitCursor)
                    ++waits;
            const int r1 = utGridStep(&st, hover(93, 177, 506, 28, 1, 14, 0, 2, 2, true, 0.0f), win1366);
            const int r2 = utGridStep(&st, hover(93, 177, 10, 10, 2, 0, 0, 2, 2, true, 1.0f), win1366);
            _snprintf_s(d, sizeof(d), _TRUNCATE, "%d waits, then %d %d, verdict %d, agree %d of %d",
                        waits, r1, r2, st.verdict, st.cursorAgree, st.cursorRead);
            ok(waits == 40 && r1 == kUtGridSamePrototype && r2 == kUtGridAccepted &&
                   st.verdict == 1 && st.gridX == 93.0f && st.gridY == 177.0f && !st.windowOk,
               "grid: 40 waits with the cursor 10 px behind, then two at rest -> accepted", d);
        }
        {   // two matches 7 cells apart do not vouch; 8 cells apart do (x, then y)
            UtGridState st;
            utGridReset(&st);
            utGridStep(&st, hover(93, 177, 10, 10, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            const int r1 = utGridStep(&st, hover(93, 177, 234, 10, 2, 7, 0, 1, 1, true, 0.0f), win1366);
            const int r2 = utGridStep(&st, hover(93, 177, 266, 10, 3, 8, 0, 1, 1, true, 0.0f), win1366);
            utGridReset(&st);
            utGridStep(&st, hover(93, 177, 10, 10, 1, 0, 0, 2, 2, true, 0.0f), win1366);
            const int r3 = utGridStep(&st, hover(93, 177, 10, 234, 2, 0, 7, 1, 1, true, 0.0f), win1366);
            const int r4 = utGridStep(&st, hover(93, 177, 10, 266, 3, 0, 8, 1, 1, true, 0.0f), win1366);
            _snprintf_s(d, sizeof(d), _TRUNCATE, "x: %d %d, y: %d %d", r1, r2, r3, r4);
            ok(r1 == kUtGridWaitCursor && r2 == kUtGridAccepted && r3 == kUtGridWaitCursor &&
                   r4 == kUtGridAccepted,
               "grid: the cursor proof needs its two matches 8 cells apart", d);
        }
        {   // check 1 fails on ONE footprint bound alone - footprint (3,4) 2x2
            static const float kP[4][2] = {{100, 100}, {100, 200}, {70, 140}, {170, 140}};
            static const char* kWhat[4] = {"row above", "row below", "column left", "column right"};
            for (int i = 0; i < 4; ++i) {
                UtGridState st;
                utGridReset(&st);
                const int r = utGridStep(&st, hover(93, 177, kP[i][0], kP[i][1], 9, 3, 4, 2, 2, true,
                                                    0.0f), win1366);
                _snprintf_s(d, sizeof(d), _TRUNCATE, "%s: point (%.0f,%.0f) -> %d", kWhat[i],
                            kP[i][0], kP[i][1], r);
                ok(r == kUtGridRefusedCell && st.verdict == -1,
                   "grid: check 1 fails on one footprint bound alone", d);
            }
            UtGridState st;
            utGridReset(&st);
            ok(utGridStep(&st, hover(93, 177, 100, 140, 9, 3, 4, 2, 2, true, 0.0f), win1366) ==
                   kUtGridFirst,
               "grid: check 1 holds inside the footprint (3,4) 2x2", nullptr);
        }
        {   // every sequence up to depth 5 over 12 hover kinds x 4 bound sets vs an oracle
            struct Kind { float gx, gy, x, y; unsigned id; int col, row, w, h; bool cur; float dx; };
            static const Kind kK[12] = {
                {93, 177, 506, 28, 1, 14, 0, 2, 2, true, 0.0f},    // A, cursor agrees
                {93, 177, 10, 10, 2, 0, 0, 2, 2, true, 2.0f},      // B, cursor agrees
                {93, 177, 40, 100, 3, 1, 3, 1, 1, false, 0.0f},    // C, no cursor
                {93, 177, 40, 100, 3, 1, 3, 1, 1, true, 30.0f},    // C, cursor far
                {93, 177, 300, 300, 4, 0, 0, 2, 2, true, 0.0f},    // check 1 fails
                {93.3f, 177, 10, 10, 2, 0, 0, 2, 2, false, 0.0f},  // B, origin within tol
                {95, 177, 10, 10, 2, 0, 0, 2, 2, true, 0.0f},      // B, another origin
                {93, 177, 10, 10, 0, 0, 0, 2, 2, true, 0.0f},      // no item
                {37, 192, 40, 40, 5, 1, 1, 1, 1, false, 0.0f},     // records' origin, no cursor
                {93, 177, 100, 100, 6, 3, 4, 2, 2, true, 0.0f},   // check 1 fails on the row alone (above)
                {93, 177, 100, 200, 7, 3, 4, 2, 2, true, 0.0f},   // ... on the row alone (below)
                {93, 177, 70, 140, 8, 3, 4, 2, 2, true, 0.0f}};   // ... on the column alone (left)
            const UtGridBounds kB[4] = {
                {1366, 768, 10, 65.5f, 565, 637},    // the measured window (the grid at 93 leaves it)
                {1366, 768, 0, 0, 1366, 768},        // a window the whole canvas
                {600, 600, 0, 0, 600, 600},          // a small canvas (the grid at 93 leaves it)
                {1366, 768, 30, 150, 560, 560}};     // a window that holds the records' origin
            long long seqs = 0, accepted = 0, bad = 0;
            char firstBad[160] = "";
            int hseq[5];
            const int n = 12, depth = 5;
            for (int bi = 0; bi < 4; ++bi) {
                for (int len = 1; len <= depth; ++len) {
                    long long total = 1;
                    for (int i = 0; i < len; ++i) total *= n;
                    for (long long c = 0; c < total; ++c) {
                        long long v = c;
                        for (int i = 0; i < len; ++i) {
                            hseq[i] = (int)(v % n);
                            v /= n;
                        }
                        ++seqs;
                        UtGridState st;
                        utGridReset(&st);
                        int counted = 0, agree = 0, decidedAt = -1, lastVerdict = 0;
                        float amnx = 0, amxx = 0, amny = 0, amxy = 0;   // the matched points
                        bool cellAll = true, sameOrigin = true, finalBroken = false;
                        unsigned ids[5] = {0, 0, 0, 0, 0};
                        float ox = 0, oy = 0;
                        for (int i = 0; i < len; ++i) {
                            const Kind& k = kK[hseq[i]];
                            UtGridHover h = {};
                            h.gridX = k.gx; h.gridY = k.gy; h.x = k.x; h.y = k.y; h.id = k.id;
                            h.col = k.col; h.row = k.row; h.w = k.w; h.h = k.h;
                            h.cw = h.ch = 32; h.cursorRead = k.cur;
                            h.cursorX = k.gx + k.x + k.dx; h.cursorY = k.gy + k.y;
                            const bool before = st.verdict != 0;
                            if (!before && k.id) {   // the oracle's own record of counted hovers
                                const int cx = (int)(k.x / 32.0f), cy = (int)(k.y / 32.0f);
                                if (!(cx >= k.col && cx < k.col + k.w && cy >= k.row &&
                                      cy < k.row + k.h))
                                    cellAll = false;
                                if (counted == 0) { ox = k.gx; oy = k.gy; }
                                else if (k.gx - ox > 0.5f || ox - k.gx > 0.5f ||
                                         k.gy - oy > 0.5f || oy - k.gy > 0.5f)
                                    sameOrigin = false;
                                if (k.cur && k.dx <= 4.0f) {
                                    if (agree == 0) { amnx = amxx = k.x; amny = amxy = k.y; }
                                    if (k.x < amnx) amnx = k.x;
                                    if (k.x > amxx) amxx = k.x;
                                    if (k.y < amny) amny = k.y;
                                    if (k.y > amxy) amxy = k.y;
                                    ++agree;
                                }
                                ids[counted++] = k.id;
                            }
                            utGridStep(&st, h, kB[bi]);
                            ++g_checks;
                            if (before && st.verdict != lastVerdict) finalBroken = true;
                            if (!before && st.verdict != 0) decidedAt = i;
                            lastVerdict = st.verdict;
                        }
                        bool distinct = false;
                        for (int i = 1; i < counted; ++i)
                            if (ids[i] != ids[0]) distinct = true;
                        const UtGridBounds& b = kB[bi];
                        const bool candCanvas = counted > 0 && ox >= 0 && oy >= 0 &&
                                                ox + 512 <= b.canvasW + 0.5f &&
                                                oy + 480 <= b.canvasH + 0.5f;
                        if (st.verdict == -1 && cellAll && sameOrigin && !(distinct && !candCanvas)) {
                            ++bad;   // a refusal needs (a), (b) or the canvas
                            if (!firstBad[0])
                                _snprintf_s(firstBad, sizeof(firstBad), _TRUNCATE,
                                            "a refusal without cause (bounds %d len %d)", bi, len);
                        }
                        if (st.verdict != 1) {
                            if (finalBroken) { ++bad; if (!firstBad[0]) _snprintf_s(firstBad, sizeof(firstBad), _TRUNCATE, "a decision changed (bounds %d)", bi); }
                            continue;
                        }
                        ++accepted;
                        const bool inCanvas = st.gridX >= 0 && st.gridY >= 0 &&
                                              st.gridX + 512 <= b.canvasW + 0.5f &&
                                              st.gridY + 480 <= b.canvasH + 0.5f;
                        const bool inWin = st.gridX >= b.winX - 0.5f && st.gridY >= b.winY - 0.5f &&
                                           st.gridX + 512 <= b.winX + b.winW + 0.5f &&
                                           st.gridY + 480 <= b.winY + b.winH + 0.5f;
                        const bool proof = agree >= 2 && (amxx - amnx >= 256.0f || amxy - amny >= 256.0f);
                        const bool good = cellAll && sameOrigin && distinct && inCanvas &&
                                          (inWin || proof) && !finalBroken &&
                                          decidedAt >= 1 && st.gridX == ox && st.gridY == oy;
                        if (!good) {
                            ++bad;
                            if (!firstBad[0])
                                _snprintf_s(firstBad, sizeof(firstBad), _TRUNCATE,
                                            "bounds %d len %d: cell %d origin %d distinct %d canvas %d win %d agree %d",
                                            bi, len, cellAll, sameOrigin, distinct, inCanvas, inWin, agree);
                        }
                    }
                }
            }
            _snprintf_s(d, sizeof(d), _TRUNCATE, "%lld sequences, %lld accepted, %lld bad%s%s", seqs,
                        accepted, bad, bad ? ": " : "", firstBad);
            ok(bad == 0 && accepted > 0, "grid: every accepted origin meets (a) (b) (c)", d);
        }
    }


    // ---- the view in multiplayer -----------------------------------------
    {
        // Every sequence of 1..7 events over {toggle, single, multiplayer, unknown} from a ready
        // page (bindings, world, caravan open on the Transfer tab), checked against the TRUE
        // session state the events carry: never ON while it is unknown; a toggle while OFF turns
        // ON iff it is known (multiplayer included); every CHANGE of it while ON turns the view
        // OFF; a repeat never does. The shipped step must pass; two mutants must not: the rule
        // (a toggle refused in multiplayer) and a step that ignores an unknown read.
        typedef integration::UtViewStep (*Apply)(integration::UtViewState*, int, int);
        struct Mut {
            static integration::UtViewStep oldRule(integration::UtViewState* s, int ev, int arg) {
                if (ev == integration::kUtViewEvToggle && !s->on && s->mp) {
                    integration::UtViewStep st = {false, false, integration::kUtViewWhyRefusedMp};
                    return st;
                }
                return integration::utViewApply(s, ev, arg);
            }
            static integration::UtViewStep ignoreUnknown(integration::UtViewState* s, int ev, int arg) {
                if (ev == integration::kUtViewEvMpChanged && arg == 2) {
                    integration::UtViewStep st = {false, false, integration::kUtViewWhyOk};
                    return st;
                }
                return integration::utViewApply(s, ev, arg);
            }
        };
        const int kEv[4][2] = {{integration::kUtViewEvToggle, 0}, {integration::kUtViewEvMpChanged, 0},
                               {integration::kUtViewEvMpChanged, 1}, {integration::kUtViewEvMpChanged, 2}};
        const Apply fns[3] = {&integration::utViewApply, &Mut::oldRule, &Mut::ignoreUnknown};
        long long bad[3] = {0, 0, 0};
        long long seqs = 0, steps = 0, onInMp = 0, changeOffs = 0, repeats = 0, subInMp = 0;
        for (int fi = 0; fi < 3; ++fi) {
            for (int len = 1; len <= 7; ++len) {
                int total = 1;
                for (int i = 0; i < len; ++i) total *= 4;
                for (int code = 0; code < total; ++code) {
                    integration::UtViewState s;
                    s.bindingsOk = s.world = s.open = true;
                    s.mode = 1;
                    int truth = 0;   // 0 single, 1 multiplayer, 2 unknown
                    bool seqOnInMp = false;
                    int c = code;
                    for (int i = 0; i < len; ++i, c /= 4) {
                        const int e = c % 4;
                        const bool wasOn = s.on;
                        const int prev = truth;
                        if (e > 0) truth = kEv[e][1];
                        const integration::UtViewStep st = fns[fi](&s, kEv[e][0], kEv[e][1]);
                        if (fi == 0) ++steps;
                        if (s.on && truth == 2) ++bad[fi];
                        if (e == 0 && !wasOn && st.turnedOn != (truth != 2)) ++bad[fi];
                        if (e > 0 && wasOn && truth != prev) {
                            if (st.turnedOff && !s.on) {
                                if (fi == 0) ++changeOffs;
                            } else {
                                ++bad[fi];
                            }
                        }
                        if (e > 0 && truth == prev) {
                            if (fi == 0) ++repeats;
                            if (st.turnedOff) ++bad[fi];
                        }
                        if (s.on && truth == 1) {
                            seqOnInMp = true;
                            if (fi == 0 && integration::utViewSubstitute(true, s.on, s.mode, s.world,
                                                                s.mpUnknown))
                                ++subInMp;
                        }
                    }
                    if (fi == 0) {
                        ++seqs;
                        if (seqOnInMp) ++onInMp;
                    }
                }
            }
        }
        _snprintf_s(d, sizeof(d), _TRUNCATE,
                    "%lld sequences, %lld steps: ON in multiplayer in %lld (the getter substitutes "
                    "at %lld steps), %lld changes while ON turned it OFF, %lld repeats left it; "
                    "mutants caught: rule %lld, ignore-unknown %lld",
                    seqs, steps, onInMp, subInMp, changeOffs, repeats, bad[1], bad[2]);
        ok(bad[0] == 0 && onInMp > 0 && subInMp > 0 && changeOffs > 0 && repeats > 0 &&
               bad[1] > 0 && bad[2] > 0,
           "the view in multiplayer: on in MP, off on a state change, refused when unknown", d);
    }
    {
        // the +0x30 capability (IsTransferCapable) while ON: single / multiplayer / unknown x
        // mp_collect 0 / 1 - GD's rule, and an unknown state refuses whatever mp_collect says
        integration::UtDepositFacts f;
        memset(&f, 0, sizeof(f));
        f.caller = integration::kUtDepCallerDrag;
        f.viewOn = f.tableOwns = f.bindings = f.isItem = f.inCatalogue = true;
        f.stack = 0;
        int rows = 0;
        for (int st = 0; st < 3; ++st)
            for (int collect = 0; collect < 2; ++collect) {
                f.mpKnown = st != 2;
                f.mp = st == 1;
                f.mpCollect = collect != 0;
                const bool cap = integration::utDepositCapable(f);
                const bool want = st == 0 || (st == 1 && collect == 1);
                const bool named = want || integration::utDepositCapableVerdict(f) ==
                                               (st == 2 ? integration::kUtDepRefuseMpUnknown
                                                        : integration::kUtDepRefuseMultiplayer);
                if (cap == want && named && integration::utCapSlotAnswer(true, cap, true) == want &&
                    integration::utCapSlotAnswer(false, cap, true))
                    ++rows;
            }
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%d of 6 rows", rows);
        ok(rows == 6,
           "30 in MP: mp_collect=1 accepts a unique, 0 refuses (multiplayer named), unknown "
           "refuses both; OFF stays vanilla", d);
    }
    ok(strcmp(integration::utMpShapeText(false, true, true, false), "?") == 0 &&
           strcmp(integration::utMpShapeText(true, false, false, false), "off") == 0 &&
           strcmp(integration::utMpShapeText(true, true, true, false), "host") == 0 &&
           strcmp(integration::utMpShapeText(true, true, false, true), "client") == 0 &&
           strcmp(integration::utMpShapeText(true, true, true, true), "host") == 0 &&
           strcmp(integration::utMpShapeText(true, true, false, false), "on") == 0 &&
           strcmp(integration::utMpShapeText(false, false, false, false), "?") == 0,
       "heartbeat mp= host / client / off / on (known MP, no role) / ? (unknown only; "
       ")", "7 rows");
    {
        // an uncollected record shows no tint, no red and no border unless hovered - over
        // every combination (collected x hovered x owned_marks 0..3 x route live): the original
        // is skipped ONLY for uncollected + not hovered + marks != 0 + route live (3 of 32)
        int rows = 0, skips = 0;
        // the gray lend's own runtime rule (utOwnedGray, which
        // ownedGrayWanted runs) matches its spec, and gray + route live implies the skip (the gray
        // icon never sits on a coloured background) - two runtime predicates, not a restatement
        int grayRows = 0, grays = 0, implied = 0;
        for (int c = 0; c < 2; ++c)
            for (int h = 0; h < 2; ++h)
                for (int m = 0; m <= 3; ++m)
                    for (int r = 0; r < 2; ++r) {
                        const bool got = integration::utBackgroundSkip(c != 0, h != 0, m, r != 0);
                        const bool want = c == 0 && h == 0 && m >= 1 && m <= 3 && r == 1;
                        const bool gray = integration::utOwnedGray(c != 0, h != 0, m);
                        if (got) ++skips;
                        if (got == want) ++rows;
                        if (gray == (m == 3 && c == 0 && h == 0)) ++grayRows;
                        if (gray) ++grays;
                        if (!(gray && r != 0) || got) ++implied;
                    }
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%d of 32 rows, %d skip", rows, skips);
        ok(rows == 32 && skips == 3,
           "background skip: only uncollected + not hovered + owned_marks 1..3 + route live; "
           "collected, hovered, marks 0 or no route draw as the engine draws", d);
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%d of 32 gray rows (%d gray), %d of 32 implied",
                    grayRows, grays, implied);
        ok(grayRows == 32 && grays == 2 && implied == 32,
           "gray lend (utOwnedGray: owned_marks 3, uncollected, not hovered) + route live => "
           "the background is skipped", d);
        // the token: -1 = the original + no Post (a fault in Pre gives -1 = no skip); the save
        // index and the skip flag round-trip; a refused widget can skip with no Post
        int trows = 0;
        const int saves[] = {-1, 0, 1, 7, 255, 1023, integration::kUtBgNoSave - 1};
        for (int k = 0; k < (int)(sizeof(saves) / sizeof(saves[0])); ++k)
            for (int sk = 0; sk < 2; ++sk) {
                const int t = integration::utBgToken(saves[k], sk != 0);
                const bool skipOk = integration::utBgTokenSkip(t) == (sk != 0);
                const bool saveOk = integration::utBgTokenSave(t) == saves[k];
                const bool plain = saves[k] < 0 && sk == 0 ? t == -1 : t >= 0;
                if (skipOk && saveOk && plain) ++trows;
            }
        const bool fault = !integration::utBgTokenSkip(-1) && integration::utBgTokenSave(-1) == -1 &&
                           integration::utBgToken(integration::kUtBgNoSave, false) == -1;
        _snprintf_s(d, sizeof(d), _TRUNCATE, "%d of 14 rows, fault %s", trows,
                    fault ? "ok" : "BAD");
        ok(trows == 14 && fault,
           "the route token carries the skip: save index and flag round-trip; -1 (a fault "
           "in Pre) = the original runs", d);
    }
    printf(g_fail ? "[viewgate]  FAILED: %d\n" : "[viewgate]  ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}