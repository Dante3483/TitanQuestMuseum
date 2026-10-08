// test_store.cpp - the private table's owning side (src\game\storage_adapter.cpp) and the pure decisions of
// src\backend\deposit_rules.h, offline. NO GAME NEEDED.
//
// TQ port of the Grim Dawn mod's tools\test_store.cpp. GD's central proof
// is kept: the REAL decision functions the shipped build calls are enumerated over EVERY fact
// combination, and every verdict but "journal it" is shown to be reached without the journal
// being touched. Added for TQ: the take decision, the save-set leaf, the store's
// deposit/take/count through a real journal file, and the character-save watch over a fake
// SaveData folder (%UNIQUETAB_SAVEDATA%).
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include "../src/core/configuration.h"
#include "../src/backend/deposit_rules.h"
#include "../src/core/logging.h"
#include "../src/backend/journal.h"
#include "../src/game/storage_adapter.h"

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what) {
    if (ok) {
        ++g_pass;
        printf("  [ok]   %s\n", what);
    } else {
        ++g_fail;
        printf("  [FAIL] %s\n", what);
    }
}

void touch(const char* path, unsigned long long ft) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    FILETIME f;
    f.dwLowDateTime = (DWORD)(ft & 0xFFFFFFFFu);
    f.dwHighDateTime = (DWORD)(ft >> 32);
    SetFileTime(h, nullptr, nullptr, &f);
    CloseHandle(h);
}

unsigned long long now() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

integration::UtReplicaCapture cap(const char* base, unsigned seed) {
    integration::UtReplicaCapture c;
    memset(&c, 0, sizeof(c));
    _snprintf_s(c.str[integration::kUtIdBase], integration::kUtIdStrMax, _TRUNCATE, "%s", base);
    c.seed = seed;
    c.stack = 1;
    integration::utJournalKey(base, c.record, sizeof(c.record));
    return c;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: test_store <empty folder under build\\test>\n");
        return 2;
    }
    char dir[MAX_PATH];
    _snprintf_s(dir, sizeof(dir), _TRUNCATE, "%s", argv[1]);
    CreateDirectoryA(dir, nullptr);
    SetEnvironmentVariableA("TITANQUESTMUSEUM_OUT", dir);
    char save[MAX_PATH];
    _snprintf_s(save, sizeof(save), _TRUNCATE, "%s\\SaveData", dir);
    SetEnvironmentVariableA("UNIQUETAB_SAVEDATA", save);
    wchar_t logPath[MAX_PATH];
    _snwprintf_s(logPath, MAX_PATH, _TRUNCATE, L"%S\\test_store.log", dir);
    integration::logInit(logPath);
    integration::logSetLevel("debug");

    printf("\n1. utDepositDecide over EVERY fact combination\n");
    {
        long total = 0, table = 0, wrongTable = 0, orderBad = 0;
        long stackOneRule = 0;   // combinations the rule (stack == 1 only) would refuse
        const unsigned stacks[3] = {0u, 1u, 2u};
        long haveRefused = 0;   // journal-ready combinations the one-copy rule refused
        for (int caller = 0; caller < 4; ++caller)
            for (unsigned bits = 0; bits < (1u << 10); ++bits)
                for (int si = 0; si < 3; ++si)
                for (unsigned rn = 0; rn < 3u; ++rn) {
                    integration::UtDepositFacts f;
                    f.caller = caller;
                    f.viewOn = (bits & 1u) != 0;
                    f.mpKnown = (bits & 2u) != 0;
                    f.mp = (bits & 4u) != 0;
                    f.tableOwns = (bits & 8u) != 0;
                    f.bindings = (bits & 16u) != 0;
                    f.isItem = (bits & 32u) != 0;
                    f.inCatalogue = (bits & 64u) != 0;
                    f.haveCap = (bits & 128u) != 0;
                    f.capMatches = (bits & 256u) != 0;
                    f.mpCollect = (bits & 512u) != 0;   // GD's mp_collect
                    f.stack = stacks[si];
                    f.rowsNow = rn;   // rows of its record already in the journal
                    const int v = integration::utDepositDecide(f);
                    ++total;
                    // GetNumberInStack answers 0 for a unique (Game.dll 0x8800 `xor eax,eax`);
                    // 0 and 1 are a single item, 2 or more a stack.
                    const bool all = f.viewOn && (caller == 1 || caller == 2) && f.mpKnown &&
                                     (!f.mp || f.mpCollect) && f.tableOwns && f.bindings && f.isItem &&
                                     f.inCatalogue && f.rowsNow == 0u && f.stack <= 1u &&
                                     f.haveCap && f.capMatches;
                    if (v == integration::kUtDepRefuseHave && f.stack <= 1u && f.haveCap && f.capMatches)
                        ++haveRefused;
                    if (v == integration::kUtDepTable) ++table;
                    if (v == integration::kUtDepTable && f.stack != 1u) ++stackOneRule;
                    if ((v == integration::kUtDepTable) != all) ++wrongTable;
                    // the FIRST failing fact names the verdict (the log reads in this order)
                    int want = integration::kUtDepTable;
                    if (!f.viewOn) want = integration::kUtDepRefuseViewOff;
                    else if (!(caller == 1 || caller == 2)) want = integration::kUtDepRefuseCaller;
                    else if (!f.mpKnown) want = integration::kUtDepRefuseMpUnknown;
                    else if (f.mp && !f.mpCollect) want = integration::kUtDepRefuseMultiplayer;
                    else if (!f.tableOwns) want = integration::kUtDepRefuseTableOff;
                    else if (!f.bindings) want = integration::kUtDepRefuseBindings;
                    else if (!f.isItem) want = integration::kUtDepRefuseNotItem;
                    else if (!f.inCatalogue) want = integration::kUtDepRefuseNotCatalogue;
                    else if (f.rowsNow > 0u) want = integration::kUtDepRefuseHave;
                    else if (f.stack >= 2u) want = integration::kUtDepRefuseStack;
                    else if (!f.haveCap || !f.capMatches) want = integration::kUtDepRefuseCapture;
                    if (v != want) ++orderBad;
                }
        char line[160];
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    "%ld combinations: %ld journal, the rest refused; journal iff every fact holds",
                    total, table);
        // 4 per session shape that allows moves - single player (mp_collect 0 or 1) and
        // multiplayer with mp_collect=1
        check(wrongTable == 0 && table == 12, line);
        check(orderBad == 0, "each refusal names the first failing fact, in the log's order");
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    "stack 0 (a unique, as the engine reports it) and 1 deposit, 2 refuses; the "
                    "rule (exactly 1) refused %ld of the %ld journal combinations", stackOneRule, table);
        check(stackOneRule == 6, line);
        _snprintf_s(line, sizeof(line), _TRUNCATE,
                    "one copy per record: a record with 1 or 2 rows (pending-in included) is "
                    "refused as 'already in the collection' - %ld otherwise-ready combinations",
                    haveRefused);
        check(haveRefused == 24 &&strcmp(integration::utDepositWhyText(integration::kUtDepRefuseHave),
                                          "already in the collection") == 0, line);
        check(integration::utStackIsSingle(0u) && integration::utStackIsSingle(1u) && !integration::utStackIsSingle(2u) &&
                  !integration::utStackIsSingle(0xFFFFFFFFu),
              "utStackIsSingle: 0 and 1 single, 2 and an unreadable count (0xFFFFFFFF) a stack");
        check(integration::utJournalStack(0u) == 1u && integration::utJournalStack(1u) == 1u &&
                  integration::utJournalStack(2u) == 2u,
              "the journal row's stack is max(1, n)");
        integration::UtDepositFacts f;
        memset(&f, 0, sizeof(f));
        f.caller = integration::kUtDepCallerDrag;
        f.viewOn = f.mpKnown = f.tableOwns = f.bindings = f.isItem = f.inCatalogue = true;
        f.stack = 0;   // what Item::GetNumberInStack answers for a unique
        check(integration::utDepositCapable(f), "the capability slot answers true for a unique (stack 0)");
        f.stack = 1;
        check(integration::utDepositCapable(f), "the capability slot answers before the capture ran");
        f.stack = 2;
        check(!integration::utDepositCapable(f) &&
                  integration::utDepositCapableVerdict(f) == integration::kUtDepRefuseStack,
              "... and refuses a stack of two, naming the stack (the one debug line's reason)");
        f.stack = 0;
        f.rowsNow = 1;
        check(!integration::utDepositCapable(f) &&
                  integration::utDepositCapableVerdict(f) == integration::kUtDepRefuseHave &&
                  !integration::utCapSlotAnswer(true, integration::utDepositCapable(f), true),
              "... and refuses a second copy of a collected record (the drag-drop does "
              "nothing, the item stays on the cursor)");
        f.rowsNow = 0;
        integration::UtTakeFacts tt = {true, true, false, true, true, true, 2u, true};
        check(integration::utTakeDecide(tt) == integration::kUtTakeOk,
              "a record already held twice stays takeable (the rule never refuses a take)");
        f.stack = 0;
        f.inCatalogue = false;
        check(!integration::utDepositCapable(f), "... and refuses a non-catalogue item (a green rare)");
        f.inCatalogue = true;
        f.mp = true;
        check(!integration::utDepositCapable(f), "... and everything in multiplayer with mp_collect=0");
        f.mpCollect = true;
        check(integration::utDepositCapable(f), "... and a unique in multiplayer with mp_collect=1");
        f.mpKnown = false;
        check(!integration::utDepositCapable(f) &&
                  integration::utDepositCapableVerdict(f) == integration::kUtDepRefuseMpUnknown,
              "... and everything while the session state is unknown, even with mp_collect=1");
        f.mpCollect = false;
        check(integration::utDepositCapableVerdict(f) == integration::kUtDepRefuseMpUnknown,
              "unknown with mp_collect=0 is named UNKNOWN too (the ini is not the cause)");
        check(strcmp(integration::utDepositWhyText(integration::kUtDepRefuseMultiplayer),
                     "a multiplayer session is active and mp_collect=0") == 0 &&
                  strstr(integration::utDepositWhyText(integration::kUtDepRefuseMpUnknown), "UNKNOWN") != nullptr &&
                  strstr(integration::utDepositWhyText(integration::kUtDepRefuseMpUnknown), "mp_collect=0") == nullptr,
              "two refusal texts, GD's two reasons (mp_collect=0 / the mode UNKNOWN)");
        f.mpKnown = true;
        f.mpCollect = false;
        f.mp = false;
        f.tableOwns = false;
        check(!integration::utDepositCapable(f), "... and everything with an unwritable journal (the earlier answer)");
    }

    printf("\n2. utTakeDecide\n");
    {
        long ok = 0, total = 0;
        for (unsigned bits = 0; bits < (1u << 8); ++bits)
            for (unsigned rows = 0; rows < 3; ++rows) {
                integration::UtTakeFacts t;
                t.viewOn = (bits & 1u) != 0;
                t.mpKnown = (bits & 2u) != 0;
                t.mp = (bits & 4u) != 0;
                t.tableOwns = (bits & 8u) != 0;
                t.isPrototype = (bits & 16u) != 0;
                t.fromRow = (bits & 32u) != 0;
                t.rowIsNewest = (bits & 64u) != 0;
                t.mpCollect = (bits & 128u) != 0;
                t.rows = rows;
                ++total;
                const bool all = t.viewOn && t.mpKnown && (!t.mp || t.mpCollect) && t.tableOwns &&
                                 t.isPrototype &&
                                 t.fromRow && rows > 0 && t.rowIsNewest;
                if ((integration::utTakeDecide(t) == integration::kUtTakeOk) == all) ++ok;
            }
        check(ok == total, "a take only for a newest-row prototype, rows > 0, writable, and not "
                           "MP unless mp_collect=1");
        integration::UtTakeFacts m = {true, true, true, true, true, true, 1u, true, true};
        const int inMp = integration::utTakeDecide(m);
        m.mpCollect = false;
        const int inMp0 = integration::utTakeDecide(m);
        m.mpCollect = true;
        m.mpKnown = false;
        const int unknown = integration::utTakeDecide(m);
        check(inMp == integration::kUtTakeOk && inMp0 == integration::kUtTakeRefuseMultiplayer &&
                  unknown == integration::kUtTakeRefuseMultiplayer,
              "a take in multiplayer: mp_collect=1 takes, 0 refuses, an unknown state refuses");
        check(integration::utMpMoves(true, false, false) == integration::kUtMpMovesOk &&
                  integration::utMpMoves(true, false, true) == integration::kUtMpMovesOk &&
                  integration::utMpMoves(true, true, true) == integration::kUtMpMovesOk &&
                  integration::utMpMoves(true, true, false) == integration::kUtMpMovesBarred &&
                  integration::utMpMoves(false, false, true) == integration::kUtMpMovesUnknown &&
                  integration::utMpMoves(false, true, false) == integration::kUtMpMovesUnknown,
              "utMpMoves: single / multiplayer / unknown x mp_collect 0 / 1 (GD's rule; unknown "
              "refuses both)");
        integration::UtTakeFacts t = {true, true, false, true, true, false, 0, true};
        check(integration::utTakeDecide(t) == integration::kUtTakeRefuseBare, "an uncollected (bare) prototype: refused");
    }

    printf("\n3. the save set\n");
    {
        char leaf[96];
        check(integration::utSaveSetLeaf(true, "", true, true, leaf, sizeof(leaf)) &&
                  strcmp(leaf, "tq-uniq-items") == 0, "main campaign -> tq-uniq-items");
        check(integration::utSaveSetLeaf(true, "", true, false, leaf, sizeof(leaf)) &&
                  strcmp(leaf, "tq-uniq-items-custom") == 0, "custom, no mod -> -custom");
        check(integration::utSaveSetLeaf(true, "My Quest: Part 2!", false, false, leaf, sizeof(leaf)) &&
                  strncmp(leaf, "tq-uniq-items-my_quest_part_2-", 30) == 0 && strlen(leaf) == 38,
              "a mod name wins (sanitised + a hash of what was lost) even with the quest unknown");
        {   // names that sanitise alike never share a journal; case alone does
            char a[96], b[96], c[96], d[96], e[96];
            const bool built = integration::utSaveSetLeaf(true, "My Mod", true, true, a, sizeof(a)) &&
                               integration::utSaveSetLeaf(true, "my-mod", true, true, b, sizeof(b)) &&
                               integration::utSaveSetLeaf(true, "my_mod", true, true, c, sizeof(c)) &&
                               integration::utSaveSetLeaf(true, "MyMod", true, true, d, sizeof(d)) &&
                               integration::utSaveSetLeaf(true, "mymod", true, true, e, sizeof(e));
            check(built && strcmp(a, b) != 0 && strcmp(a, c) != 0 && strcmp(b, c) != 0,
                  "'My Mod' / 'my-mod' / 'my_mod' -> three different journals");
            check(built && strcmp(d, e) == 0 && strcmp(d, "tq-uniq-items-mymod") == 0,
                  "'MyMod' = 'mymod' (a folder name ignores case), no hash when nothing was lost");
        }
        check(integration::utSaveSetLeaf(true, "Custom", true, true, leaf, sizeof(leaf)) &&
                  strcmp(leaf, "tq-uniq-items-mod_custom") == 0, "a mod called Custom never collides");
        check(!integration::utSaveSetLeaf(false, "", true, true, leaf, sizeof(leaf)), "mod name unreadable -> UNKNOWN");
        check(!integration::utSaveSetLeaf(true, "", false, true, leaf, sizeof(leaf)), "quest unreadable -> UNKNOWN");
        check(!integration::utSaveSetLeaf(true, "???", true, true, leaf, sizeof(leaf)), "a name with nothing usable -> UNKNOWN");
        char longName[200];
        memset(longName, 'a', sizeof(longName));
        longName[199] = 0;
        check(integration::utSaveSetLeaf(true, longName, true, true, leaf, sizeof(leaf)) &&
                  strlen(leaf) == 14 + 48 + 9 && leaf[14 + 48] == '-',
              "a long mod name is cut to 48 characters + a hash of the whole name");
        char longName2[200];
        memcpy(longName2, longName, sizeof(longName2));
        longName2[150] = 'b';
        char leaf2[96];
        check(integration::utSaveSetLeaf(true, longName2, true, true, leaf2, sizeof(leaf2)) &&
                  strcmp(leaf, leaf2) != 0, "two long names sharing 48 characters differ");
    }

    printf("\n4. the store over a real journal\n");
    {
        integration::g_cfg.exportCsv = 0;
        integration::storeInit(nullptr);
        check(!integration::storeTableOwns(), "no set before a world");
        unsigned long long seq = 0;
        const char* base = "records\\item\\equipmentring\\u_n_ring_01.dbr";
        check(!integration::storeOnDeposit(cap(base, 1), &seq), "a deposit without a set is refused");
        check(!integration::storeSelectSet(true, "", false, false), "an unknown set: moves refused");
        char p[MAX_PATH];
        _snprintf_s(p, sizeof(p), _TRUNCATE, "%s\\tq-uniq-items.jsonl", dir);
        DeleteFileA(p);
        check(integration::storeSelectSet(true, "", true, true) && integration::storeTableOwns(), "the main set opens");
        check(integration::storeOnDeposit(cap(base, 1), &seq) && integration::storeOnDeposit(cap(base, 2), &seq),
              "two deposits");
        check(integration::storeCount("Records/Item/EquipmentRing/U_N_Ring_01.dbr") == 2,
              "storeCount folds any spelling: 2 rows");
        check(!integration::storeOnTake(base, seq - 1), "a take of the older row is refused");
        check(!integration::storeTakeAllowed() && !integration::storeOnTake(base, seq) && integration::storeCount(base) == 2,
              "no Player.chr visible -> takes refused (deposits stay)");
        {
            char d[MAX_PATH], f[MAX_PATH];
            CreateDirectoryA(save, nullptr);
            _snprintf_s(d, sizeof(d), _TRUNCATE, "%s\\Main", save);
            CreateDirectoryA(d, nullptr);
            _snprintf_s(d, sizeof(d), _TRUNCATE, "%s\\Main\\_Hero", save);
            CreateDirectoryA(d, nullptr);
            _snprintf_s(f, sizeof(f), _TRUNCATE, "%s\\Player.chr", d);
            touch(f, now() - 600000000ull);
        }
        integration::storeSaveCheck(true);
        check(integration::storeTakeAllowed(), "a Player.chr appears -> takes allowed");
        check(integration::journalPendingIn() == 2, "... and that OLDER save settled nothing");
        check(integration::storeOnTake(base, seq) && integration::storeCount(base) == 1, "the newest is taken: 1 row");
    }

    printf("\n5. the character-save watch (read only)\n");
    {
        char main[MAX_PATH], chr[MAX_PATH], chr2[MAX_PATH];
        CreateDirectoryA(save, nullptr);
        _snprintf_s(main, sizeof(main), _TRUNCATE, "%s\\Main", save);
        CreateDirectoryA(main, nullptr);
        _snprintf_s(chr, sizeof(chr), _TRUNCATE, "%s\\Main\\_Hero", save);
        CreateDirectoryA(chr, nullptr);
        _snprintf_s(chr2, sizeof(chr2), _TRUNCATE, "%s\\User", save);
        CreateDirectoryA(chr2, nullptr);
        _snprintf_s(chr2, sizeof(chr2), _TRUNCATE, "%s\\User\\_Other", save);
        CreateDirectoryA(chr2, nullptr);
        char f1[MAX_PATH], f2[MAX_PATH];
        _snprintf_s(f1, sizeof(f1), _TRUNCATE, "%s\\Player.chr", chr);
        _snprintf_s(f2, sizeof(f2), _TRUNCATE, "%s\\Player.chr", chr2);
        const unsigned long long old = now() - 600000000ull;   // a minute ago
        touch(f1, old);
        touch(f2, old - 10000000ull);
        unsigned long long t = 0;
        check(integration::storeNewestCharacterSave(&t) && t == old, "the newest Player.chr of Main and User");
        check(integration::journalPendingIn() == 1, "one row pending in (the deposit above, not the taken one)");
        integration::storeSaveCheck(true);
        check(integration::journalPendingIn() == 1, "a save OLDER than the row settles nothing");
        touch(f2, now() + 10000000ull);
        integration::storeSaveCheck(true);
        check(integration::journalPendingIn() == 0, "a newer character save settles it");
    }

    printf("\n6. the right-click take's settle (no cursor: touched at the hand-out)\n");
    {
        unsigned long long seq = 0;
        const char* rc = "records\\item\\equipmentring\\u_n_ring_02.dbr";
        check(integration::storeCount(rc) == 0 && integration::storeOnDeposit(cap(rc, 7), &seq) &&
                  integration::storeCount(rc) == 1,
              "a first copy deposits (the one-copy rule never refuses a first deposit)");
        integration::journalSaveObserved(now() + 10000000ull);
        check(integration::journalPendingIn() == 0, "a newer save settles the deposit");
        const size_t out0 = integration::journalPendingOut();
        check(integration::storeOnTake(rc, seq) && integration::storeCount(rc) == 0 &&
                  integration::journalPendingOut() == out0 + 1,
              "the right-click take journals the row out first (viewTakeRightClick, G7)");
        check(integration::journalTouchOut(seq), "the hand-out touches the out row (viewTakeRemove)");
        integration::journalSaveObserved(now() - 10000000ull);
        check(integration::journalPendingOut() == out0 + 1, "a save OLDER than the hand-out settles nothing");
        integration::journalSaveObserved(now() + 20000000ull);
        check(integration::journalPendingOut() == 0 && integration::storeCount(rc) == 0,
              "a newer save settles it: the item is in the character file, no row, no duplicate");
    }

    integration::journalOpenSet(nullptr);
    integration::logShutdown();
    printf("\n%s - %d passed, %d failed\n", g_fail ? "STORE TESTS FAILED" : "ALL PASS", g_pass,
           g_fail);
    return g_fail ? 1 : 0;
}
