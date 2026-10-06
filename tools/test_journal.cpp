// test_journal.cpp - the journal (src\backend\journal.cpp, format TQ-1), offline. NO GAME NEEDED.
//
// TQ port of the Grim Dawn mod's tools\test_journal.cpp. GD's harness
// rules are kept: the harness runs ENTIRELY inside build\test (the folder in argv[1] becomes
// %TITANQUESTMUSEUM_OUT% BEFORE the first path call, so the mod-folder resolver can never reach a real
// installation); every case uses its own save-set leaf; nothing launches the game.
//
// What it proves: the format gate (a newer format and a foreign header
// are READ-ONLY and the bytes on disk stay untouched), the atomic write (no .tmp left, the file
// always parses), the folded key, duplicate identities kept as separate rows, a take removes the
// NEWEST row, the crash window (pending in / out, the restore at load, the save observation, the
// cancel of an unsaved take), the round trip of every field, the capture validation, a share-locked
// file, a bad line costing one row with the copy aside, and the CSV export.
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include <string>

#include "../src/backend/deposit_rules.h"   // utJournalStack
#include "../src/core/logging.h"
#include "../src/backend/journal.h"
#define UT_PROTO_PURE_ONLY
#include "../src/game/item_adapter.h"          // the take's row -> replica -> row (utReplicaFromIdentity)

#include <vector>

namespace {

int g_pass = 0, g_fail = 0;
char g_dir[MAX_PATH];

void check(bool ok, const char* what) {
    if (ok) {
        ++g_pass;
        printf("  [ok]   %s\n", what);
    } else {
        ++g_fail;
        printf("  [FAIL] %s\n", what);
    }
}

std::string readAll(const char* path) {
    std::string s;
    FILE* f = nullptr;
    if (fopen_s(&f, path, "rb") != 0 || !f) return s;
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

void writeAll(const char* path, const std::string& s) {
    FILE* f = nullptr;
    if (fopen_s(&f, path, "wb") != 0 || !f) return;
    fwrite(s.data(), 1, s.size(), f);
    fclose(f);
}

void pathOf(const char* leaf, const char* ext, char* out) {
    _snprintf_s(out, MAX_PATH, _TRUNCATE, "%s\\%s.%s", g_dir, leaf, ext);
}

void clean(const char* leaf) {
    char p[MAX_PATH];
    pathOf(leaf, "jsonl", p);
    DeleteFileA(p);
    pathOf(leaf, "csv", p);
    DeleteFileA(p);
    char glob[MAX_PATH];
    _snprintf_s(glob, sizeof(glob), _TRUNCATE, "%s\\%s.jsonl.bad-*", g_dir, leaf);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(glob, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char q[MAX_PATH];
            _snprintf_s(q, sizeof(q), _TRUNCATE, "%s\\%s", g_dir, fd.cFileName);
            DeleteFileA(q);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
}

int countBad(const char* leaf) {
    char glob[MAX_PATH];
    _snprintf_s(glob, sizeof(glob), _TRUNCATE, "%s\\%s.jsonl.bad-*", g_dir, leaf);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(glob, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    int n = 0;
    do ++n; while (FindNextFileA(h, &fd));
    FindClose(h);
    return n;
}

bool exists(const char* p) { return GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES; }

integration::UtReplicaCapture cap(const char* base, unsigned seed, const char* prefix = "",
                         const char* relic = "") {
    integration::UtReplicaCapture c;
    memset(&c, 0, sizeof(c));
    _snprintf_s(c.str[integration::kUtIdBase], integration::kUtIdStrMax, _TRUNCATE, "%s", base);
    _snprintf_s(c.str[integration::kUtIdPrefix], integration::kUtIdStrMax, _TRUNCATE, "%s", prefix);
    _snprintf_s(c.str[integration::kUtIdRelic], integration::kUtIdStrMax, _TRUNCATE, "%s", relic);
    c.seed = seed;
    c.var1 = 0;
    c.var2 = 0;
    c.b8 = 0;
    c.stack = 1;
    integration::utJournalKey(base, c.record, sizeof(c.record));
    return c;
}

bool sameCap(const integration::UtReplicaCapture& a, const integration::UtReplicaCapture& b) {
    for (int k = 0; k < integration::kUtIdStrCount; ++k)
        if (strcmp(a.str[k], b.str[k]) != 0) return false;
    return strcmp(a.record, b.record) == 0 && a.seed == b.seed && a.var1 == b.var1 &&
           a.var2 == b.var2 && a.b8 == b.b8 && a.stack == b.stack;
}

unsigned long long now() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    return ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
}

const char* kRing = "records\\item\\equipmentring\\u_n_ring_01.dbr";
const char* kRingKey = "records/item/equipmentring/u_n_ring_01.dbr";

// ---- the all-items file (tools\make_all_items.py) --------------------------------------------
// The generator's seed: FNV-1a (32 bit) over the folded key, folded into 1..32767.
unsigned allItemsSeed(const char* key) {
    unsigned h = 0x811C9DC5u;
    for (const unsigned char* p = (const unsigned char*)key; *p; ++p) h = (h ^ *p) * 0x01000193u;
    return (h % 0x7FFFu) + 1u;
}

// The JSON text appendRowLine writes for one string (the same escaping, for the ASCII the reader
// accepts: printable only, so only '"' and '\\' are escaped).
std::string jsonText(const char* s) {
    std::string o = "\"";
    for (; *s; ++s) {
        if (*s == '"' || *s == '\\') o.push_back('\\');
        o.push_back(*s);
    }
    return o + "\"";
}

// The identity part of a row line, exactly as appendRowLine writes it (stack .. b8).
std::string identityText(const integration::UtReplicaCapture& c) {
    char num[160];
    std::string o;
    _snprintf_s(num, sizeof(num), _TRUNCATE, "\"stack\":%u", c.stack);
    o += num;
    for (int k = 0; k < integration::kUtIdStrCount; ++k)
        o += "," + jsonText(integration::kUtIdStrKey[k]) + ":" + jsonText(c.str[k]);
    _snprintf_s(num, sizeof(num), _TRUNCATE, ",\"seed\":%u,\"var1\":%u,\"var2\":%u,\"b8\":%u", c.seed,
                c.var1, c.var2, c.b8);
    return o + num;
}

// A generated row line, byte for byte what make_all_items.py writes for `base` (backslashes).
std::string allItemsRow(const char* base, const char* stamp) {
    char key[260];
    integration::utJournalKey(base, key, sizeof(key));
    char num[96];
    _snprintf_s(num, sizeof(num), _TRUNCATE, ",\"seed\":%u,\"var1\":0,\"var2\":0,\"b8\":0}\n",
                allItemsSeed(key));
    return "{\"record\":" + jsonText(key) + ",\"deposited\":\"" + stamp + "\",\"stack\":1,\"base\":" +
           jsonText(base) + ",\"prefix\":\"\",\"suffix\":\"\",\"relic\":\"\",\"relicBonus\":\"\"," +
           "\"relic2\":\"\",\"relicBonus2\":\"\"" + num;
}

// The take's path for the NEWEST row of `key`: row -> replica bytes (utReplicaFromIdentity, what
// the take hands Item::CreateItem) -> identity read back out of the bytes -> the same row.
bool takeRoundTrip(const char* key, integration::UtReplicaCapture* row) {
    unsigned long long seq = 0;
    if (!integration::journalNewest(key, row, &seq)) return false;
    integration::UtReplica rep;
    integration::UtReplicaCapture back;
    if (!integration::utReplicaFromIdentity(&rep, *row) || !integration::utReplicaReadIdentity(rep.bytes, &back))
        return false;
    back.stack = row->stack;   // the stack is the row's, not the replica's
    return sameCap(*row, back) && strcmp(back.record, key) == 0;
}

// The raw text of a string value `"name":"..."` in a flat line (escapes undone), or "".
std::string fieldText(const std::string& line, const char* name) {
    const std::string pat = std::string("\"") + name + "\":\"";
    size_t p = line.find(pat);
    if (p == std::string::npos) return std::string();
    std::string v;
    for (p += pat.size(); p < line.size() && line[p] != '"'; ++p) {
        if (line[p] == '\\' && p + 1 < line.size()) ++p;
        v.push_back(line[p]);
    }
    return v;
}

unsigned headerNumber(const std::string& line, const char* name) {
    const std::string pat = std::string("\"") + name + "\":";
    const size_t p = line.find(pat);
    return p == std::string::npos ? 0xFFFFFFFFu
                                  : (unsigned)strtoul(line.c_str() + p + pat.size(), nullptr, 10);
}

// test_journal <dir> --load <file>: a COPY of the file is opened as a save set in <dir> through the
// mod's own reader; the rows, collected, pending and dropped counts against the file's own, and
// the take's round trip for the newest row of every record (identity fields exact against the
// file's bytes). Exit 0 only when nothing was dropped and every record round-trips.
int loadMode(const char* file) {
    const std::string text = readAll(file);
    if (text.empty()) {
        printf("load: %s is missing or empty\n", file);
        return 1;
    }
    std::vector<std::string> lines;
    for (size_t at = 0; at < text.size();) {
        size_t nl = text.find('\n', at);
        if (nl == std::string::npos) nl = text.size();
        std::string l = text.substr(at, nl - at);
        while (!l.empty() && l.back() == '\r') l.pop_back();
        if (l.find_first_not_of(" \t") != std::string::npos) lines.push_back(l);
        at = nl + 1;
    }
    const size_t fileRows = lines.empty() ? 0 : lines.size() - 1;
    const char* leaf = "tq-uniq-items-load";
    clean(leaf);
    char path[MAX_PATH];
    pathOf(leaf, "jsonl", path);
    writeAll(path, text);
    const bool opened = integration::journalOpenSet(leaf) && integration::journalUsable();
    const size_t rows = integration::journalCount();
    const unsigned collected = integration::journalCollectedTotal();
    const size_t pin = integration::journalPendingIn(), pout = integration::journalPendingOut();
    printf("load: %s\n", file);
    printf("  file    %u rows; header entries %u collected %u pendingIn %u pendingOut %u\n",
           (unsigned)fileRows, headerNumber(lines[0], "entries"), headerNumber(lines[0], "collected"),
           headerNumber(lines[0], "pendingIn"), headerNumber(lines[0], "pendingOut"));
    printf("  read    %s; %u rows, %u collected, %u pending in, %u pending out\n",
           opened ? "opened" : "NOT OPENED", (unsigned)rows, collected, (unsigned)pin, (unsigned)pout);
    // Per record: the lines naming it, the newest (last) one, and whether the reader kept them all.
    std::vector<std::string> keys;
    std::vector<size_t> newest, nLines;
    size_t firstDropped = 0, unnamed = 0;
    for (size_t i = 1; i < lines.size(); ++i) {
        const std::string k = fieldText(lines[i], "record");
        if (k.empty()) {
            if (!firstDropped) firstDropped = i + 1;
            ++unnamed;
            continue;
        }
        size_t j = 0;
        while (j < keys.size() && keys[j] != k) ++j;   // a few thousand keys: linear is fine
        if (j == keys.size()) {
            keys.push_back(k);
            newest.push_back(i);
            nLines.push_back(0);
        }
        newest[j] = i;
        ++nLines[j];
    }
    size_t same = 0, shortKeys = 0;
    for (size_t j = 0; j < keys.size(); ++j) {
        if (integration::journalRows(keys[j].c_str()) < nLines[j]) {
            ++shortKeys;
            if (!firstDropped || newest[j] + 1 < firstDropped) firstDropped = newest[j] + 1;
        }
        integration::UtReplicaCapture row;
        if (takeRoundTrip(keys[j].c_str(), &row) &&
            lines[newest[j]].find("," + identityText(row)) != std::string::npos)
            ++same;
    }
    const size_t dropped = fileRows - (rows < fileRows ? rows : fileRows);
    printf("  dropped %u%s", (unsigned)dropped, (dropped || shortKeys || unnamed) ? "" : "\n");
    if (dropped || shortKeys || unnamed)
        printf(" - the first at line %u: %.160s\n", (unsigned)firstDropped,
               firstDropped ? lines[firstDropped - 1].c_str() : "?");
    printf("  records %u; the take's round trip (row -> replica -> row, identity exact against the "
           "file's bytes): %u of %u\n",
           (unsigned)keys.size(), (unsigned)same, (unsigned)keys.size());
    integration::journalOpenSet(nullptr);
    const bool ok = opened && rows == fileRows && dropped == 0 && shortKeys == 0 && unnamed == 0 &&
                    headerNumber(lines[0], "entries") == rows &&
                    headerNumber(lines[0], "collected") == collected && same == keys.size();
    printf("%s\n", ok ? "LOAD OK" : "LOAD FAILED");
    return ok ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("usage: test_journal <empty folder under build\\test>\n");
        return 2;
    }
    _snprintf_s(g_dir, sizeof(g_dir), _TRUNCATE, "%s", argv[1]);
    CreateDirectoryA(g_dir, nullptr);
    SetEnvironmentVariableA("TITANQUESTMUSEUM_OUT", g_dir);   // BEFORE any path call
    wchar_t logPath[MAX_PATH];
    _snwprintf_s(logPath, MAX_PATH, _TRUNCATE, L"%S\\test_journal.log", g_dir);
    integration::logInit(logPath);
    integration::logSetLevel("debug");
    if (argc >= 4 && strcmp(argv[2], "--load") == 0) {
        const bool init = integration::journalInit(nullptr);
        const int rc = init ? loadMode(argv[3]) : 1;
        integration::logShutdown();
        return rc;
    }
    check(integration::journalInit(nullptr), "journalInit resolves the folder and opens no file");

    printf("\nA. no save set: nothing is usable, nothing is written\n");
    check(!integration::journalSetKnown() && !integration::journalUsable(), "no set -> not usable");
    {
        unsigned long long seq = 0;
        bool rb = false;
        check(!integration::journalDepositCommit(cap(kRing, 5), &seq, &rb), "a deposit is refused");
        check(integration::journalRows(kRingKey) == 0 && integration::journalCount() == 0, "no row appeared");
    }
    check(!integration::journalOpenSet("Bad Leaf!"), "an unsafe set name is refused (unknown set)");
    check(!integration::journalUsable(), "and nothing is usable");

    printf("\nB. a first deposit: the file, its header, the pending row\n");
    clean("tq-uniq-items-ta");
    char path[MAX_PATH];
    pathOf("tq-uniq-items-ta", "jsonl", path);
    check(integration::journalOpenSet("tq-uniq-items-ta") && integration::journalUsable(), "set opened, usable");
    check(!exists(path), "no file before the first write");
    {
        unsigned long long seq = 0;
        bool rb = true;
        const bool ok = integration::journalDepositCommit(cap(kRing, 12345, "records\\item\\lootmagicalaffixes\\prefix\\a.dbr"), &seq, &rb);
        check(ok && seq != 0 && !rb, "deposit committed");
        const std::string t = readAll(path);
        check(t.find("{\"journal\":\"titan quest uniquetab\",\"format\":1,") == 0,
              "line 1 names Titan Quest and format 1");
        check(t.find("\"set\":\"tq-uniq-items-ta\"") != std::string::npos, "the header names the set");
        check(t.find("\"record\":\"records/item/equipmentring/u_n_ring_01.dbr\"") != std::string::npos,
              "the row's key is the folded base");
        check(t.find("\"base\":\"records\\\\item\\\\equipmentring\\\\u_n_ring_01.dbr\"") != std::string::npos,
              "the raw base is kept byte-exact (backslashes escaped)");
        check(t.find("\"pending\":\"in\"") != std::string::npos, "the row is pending (no save seen)");
        char tmp[MAX_PATH];
        _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%s.tmp", path);
        check(!exists(tmp), "atomic write: no .tmp left behind");
        check(integration::journalRows(kRingKey) == 1 && integration::journalCollectedTotal() == 1, "rows = 1");
    }

    printf("\nC. the folded key: case and path separators\n");
    {
        check(integration::journalRows("records/item/equipmentring/u_n_ring_01.dbr") == 1, "the folded key finds it");
        char k[256];
        check(integration::utJournalKey("Records\\Item\\EquipmentRing\\U_N_Ring_01.DBR", k, sizeof(k)) &&
                  strcmp(k, kRingKey) == 0, "any spelling folds to the same key");
        unsigned long long seq = 0;
        bool rb = false;
        check(integration::journalDepositCommit(cap("Records\\Item\\EquipmentRing\\U_N_Ring_01.DBR", 7), &seq, &rb),
              "a deposit spelled differently");
        check(integration::journalRows(kRingKey) == 2, "... counts under the same key (2 rows)");
        const std::string t = readAll(path);
        check(t.find("\"base\":\"Records\\\\Item\\\\EquipmentRing\\\\U_N_Ring_01.DBR\"") != std::string::npos,
              "and its raw base keeps its own spelling");
    }

    printf("\nD. duplicate identities are separate rows\n");
    {
        unsigned long long s1 = 0, s2 = 0;
        bool rb = false;
        check(integration::journalDepositCommit(cap(kRing, 999), &s1, &rb) &&
                  integration::journalDepositCommit(cap(kRing, 999), &s2, &rb) && s1 != s2,
              "the same identity deposited twice");
        check(integration::journalRows(kRingKey) == 4, "4 rows, never merged");
        check(integration::journalOpenSet("tq-uniq-items-tb") && integration::journalOpenSet("tq-uniq-items-ta"),
              "switch set and back (re-read from disk)");
        check(integration::journalRows(kRingKey) == 4 && integration::journalCount() == 4, "still 4 rows after the re-read");
    }

    printf("\nE. a take removes the NEWEST row; a stale take is refused\n");
    {
        integration::UtReplicaCapture n;
        unsigned long long seq = 0;
        check(integration::journalNewest(kRingKey, &n, &seq) && n.seed == 999, "the newest row is the last deposit");
        bool rb = false;
        check(!integration::journalTakeCommit(kRingKey, seq - 1, &rb), "a take of an older row is refused (stale page)");
        check(integration::journalRows(kRingKey) == 4, "... nothing changed");
        check(integration::journalTakeCommit(kRingKey, seq, &rb), "the take of the newest row");
        check(integration::journalRows(kRingKey) == 3, "rows 4 -> 3");
        check(integration::journalCount() == 3, "an UNSAVED deposit taken back is erased (both states agree)");
        unsigned long long seq2 = 0;
        check(integration::journalNewest(kRingKey, &n, &seq2) && n.seed == 999 && seq2 != seq,
              "the next newest is the other seed-999 copy");
    }

    printf("\nF. the crash window: a save settles; an unsaved take is restored at load\n");
    {
        check(integration::journalPendingIn() == 3, "3 rows pending in");
        check(integration::journalSaveObserved(now() + 10) == 3, "a character save after them settles all 3");
        check(integration::journalPendingIn() == 0, "none pending");
        integration::journalFlushNow();
        integration::UtReplicaCapture n;
        unsigned long long seq = 0;
        integration::journalNewest(kRingKey, &n, &seq);
        bool rb = false;
        check(integration::journalTakeCommit(kRingKey, seq, &rb), "take a SAVED row");
        check(integration::journalRows(kRingKey) == 2 && integration::journalCount() == 3 && integration::journalPendingOut() == 1,
              "it stops counting but stays in the file as \"out\"");
        check(readAll(path).find("\"pending\":\"out\"") != std::string::npos, "on disk: pending out + taken");
        // the crash: reopen with no save observed
        check(integration::journalOpenSet("tq-uniq-items-tb") && integration::journalOpenSet("tq-uniq-items-ta"),
              "reload (a crash before the character save)");
        check(integration::journalRows(kRingKey) == 3 && integration::journalPendingOut() == 0,
              "the take is RESTORED: a duplicate at worst, never a loss");
        integration::journalNewest(kRingKey, &n, &seq);
        check(integration::journalTakeCommit(kRingKey, seq, &rb), "take it again");
        check(integration::journalSaveObserved(now() - 100000000ull) == 0, "a save OLDER than the take settles nothing");
        check(integration::journalSaveObserved(now() + 10) == 1 && integration::journalCount() == 2,
              "a newer save drops the taken row for good");
    }

    printf("\nG. a deposit of the identity of an unsaved take cancels the take\n");
    {
        integration::UtReplicaCapture n;
        unsigned long long seq = 0;
        integration::journalNewest(kRingKey, &n, &seq);
        bool rb = false;
        check(integration::journalTakeCommit(kRingKey, seq, &rb) && integration::journalPendingOut() == 1, "take a saved row");
        unsigned long long s2 = 0;
        check(integration::journalDepositCommit(n, &s2, &rb) && s2 == seq, "deposit the same identity back");
        check(integration::journalCount() == 2 && integration::journalPendingOut() == 0 && integration::journalPendingIn() == 0,
              "no new row: the take is cancelled, the row settled again");
    }

    printf("\nH. every field survives the round trip\n");
    {
        clean("tq-uniq-items-tc");
        check(integration::journalOpenSet("tq-uniq-items-tc"), "set tc");
        integration::UtReplicaCapture c = cap("records\\xpack\\item\\u \"q\"\\x.dbr", 0xFFFFFFFFu,
                                     "records\\p\\a,b.dbr", "records\\relic\\r.dbr");
        _snprintf_s(c.str[integration::kUtIdSuffix], integration::kUtIdStrMax, _TRUNCATE, "records\\s\\{x}.dbr");
        _snprintf_s(c.str[integration::kUtIdRelicBonus], integration::kUtIdStrMax, _TRUNCATE, "bonus/1");
        _snprintf_s(c.str[integration::kUtIdRelic2], integration::kUtIdStrMax, _TRUNCATE, "relic2");
        _snprintf_s(c.str[integration::kUtIdRelicBonus2], integration::kUtIdStrMax, _TRUNCATE, "bonus2 \\\\ end");
        c.var1 = 0x12345678u;
        c.var2 = 0x80000001u;
        c.b8 = 255;
        unsigned long long seq = 0;
        bool rb = false;
        check(integration::journalDepositCommit(c, &seq, &rb), "deposit with quotes, commas, braces, max u32, b8 255");
        check(integration::journalOpenSet("tq-uniq-items-ta") && integration::journalOpenSet("tq-uniq-items-tc"), "re-read");
        integration::UtReplicaCapture back;
        check(integration::journalNewest(c.record, &back, nullptr) && sameCap(back, c),
              "the row read back equals the capture, field for field");
    }

    printf("\nI. the capture's own check refuses a row that must not be written\n");
    {
        char why[160];
        integration::UtReplicaCapture c = cap(kRing, 1);
        check(integration::utCaptureValid(c, why, sizeof(why)), "a good capture passes");
        _snprintf_s(c.record, sizeof(c.record), _TRUNCATE, "records/item/other.dbr");
        check(!integration::utCaptureValid(c, why, sizeof(why)), "a key that is not the folded base is refused");
        c = cap(kRing, 1);
        c.str[integration::kUtIdPrefix][0] = '\x01';
        check(!integration::utCaptureValid(c, why, sizeof(why)), "a non-printable string is refused");
        c = cap(kRing, 1);
        c.b8 = 256;
        check(!integration::utCaptureValid(c, why, sizeof(why)), "b8 > 255 is refused");
        c = cap("", 1);
        check(!integration::utCaptureValid(c, why, sizeof(why)), "an empty base is refused");
        const size_t before = integration::journalCount();
        unsigned long long seq = 0;
        bool rb = false;
        c = cap(kRing, 1);
        c.stack = 0;
        check(!integration::journalDepositCommit(c, &seq, &rb) && integration::journalCount() == before,
              "journalDepositCommit refuses it and adds nothing");
        // the capture writes utJournalStack(GetNumberInStack) - a unique's 0 becomes 1.
        check(integration::utJournalStack(0u) == 1u, "the capture's stack for a unique (0) is 1 in the row");
    }

    printf("\nJ. the format gate: a newer file is READ-ONLY and never overwritten\n");
    {
        clean("tq-uniq-items-td");
        char p[MAX_PATH];
        pathOf("tq-uniq-items-td", "jsonl", p);
        const std::string newer =
            "{\"journal\":\"titan quest uniquetab\",\"format\":2,\"entries\":1}\n"
            "{\"record\":\"records/item/equipmentring/u_n_ring_01.dbr\",\"stack\":1,"
            "\"base\":\"records\\\\item\\\\equipmentring\\\\u_n_ring_01.dbr\",\"prefix\":\"\","
            "\"suffix\":\"\",\"relic\":\"\",\"relicBonus\":\"\",\"relic2\":\"\",\"relicBonus2\":\"\","
            "\"seed\":1,\"var1\":0,\"var2\":0,\"b8\":0,\"futureField\":\"x\"}\n";
        writeAll(p, newer);
        check(integration::journalOpenSet("tq-uniq-items-td"), "the set opens");
        check(integration::journalReadOnly() && !integration::journalUsable(), "format 2 > 1: READ-ONLY this session");
        check(integration::journalRows(kRingKey) == 1, "what could be read is shown (display stays)");
        unsigned long long seq = 0;
        bool rb = false;
        check(!integration::journalDepositCommit(cap(kRing, 3), &seq, &rb), "a deposit is refused");
        integration::UtReplicaCapture n;
        integration::journalNewest(kRingKey, &n, &seq);
        check(!integration::journalTakeCommit(kRingKey, seq, &rb), "a take is refused");
        check(!integration::journalFlushNow(), "no write at all");
        check(readAll(p) == newer, "the file's bytes are untouched");
    }

    printf("\nK. a foreign journal (GD's header) is never read as ours\n");
    {
        clean("tq-uniq-items-te");
        char p[MAX_PATH];
        pathOf("tq-uniq-items-te", "jsonl", p);
        const std::string gd = "{\"journal\":\"grim dawn uniquetab\",\"format\":4,\"entries\":0}\n";
        writeAll(p, gd);
        check(integration::journalOpenSet("tq-uniq-items-te") && integration::journalReadOnly(), "READ-ONLY");
        check(integration::journalCount() == 0 && readAll(p) == gd, "nothing read, the bytes untouched");
    }

    printf("\nL. a file that is there and cannot be read is never treated as empty\n");
    {
        clean("tq-uniq-items-tf");
        char p[MAX_PATH];
        pathOf("tq-uniq-items-tf", "jsonl", p);
        writeAll(p, "{\"journal\":\"titan quest uniquetab\",\"format\":1,\"entries\":0}\n");
        HANDLE lock = CreateFileA(p, GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        check(lock != INVALID_HANDLE_VALUE, "an exclusive handle holds the file");
        check(integration::journalOpenSet("tq-uniq-items-tf") && integration::journalReadOnly() && !integration::journalUsable(),
              "share-locked -> READ-ONLY, moves refused");
        if (lock != INVALID_HANDLE_VALUE) CloseHandle(lock);
        std::string empty = "";
        writeAll(p, empty);
        check(integration::journalOpenSet("tq-uniq-items-ta") && integration::journalOpenSet("tq-uniq-items-tf") &&
                  integration::journalReadOnly(), "a zero-byte file -> READ-ONLY too");
    }

    printf("\nM. a bad line costs that row only, and the file is copied aside first\n");
    {
        clean("tq-uniq-items-tg");
        char p[MAX_PATH];
        pathOf("tq-uniq-items-tg", "jsonl", p);
        const std::string row =
            "{\"record\":\"records/item/equipmentring/u_n_ring_01.dbr\",\"stack\":1,"
            "\"base\":\"records\\\\item\\\\equipmentring\\\\u_n_ring_01.dbr\",\"prefix\":\"\","
            "\"suffix\":\"\",\"relic\":\"\",\"relicBonus\":\"\",\"relic2\":\"\",\"relicBonus2\":\"\","
            "\"seed\":1,\"var1\":0,\"var2\":0,\"b8\":0}\n";
        const std::string wrongKey =
            "{\"record\":\"records/item/other.dbr\",\"stack\":1,"
            "\"base\":\"records\\\\item\\\\equipmentring\\\\u_n_ring_01.dbr\",\"prefix\":\"\","
            "\"suffix\":\"\",\"relic\":\"\",\"relicBonus\":\"\",\"relic2\":\"\",\"relicBonus2\":\"\","
            "\"seed\":1,\"var1\":0,\"var2\":0,\"b8\":0}\n";
        writeAll(p, "{\"journal\":\"titan quest uniquetab\",\"format\":1,\"entries\":4}\n" + row +
                        "{not json\n" + wrongKey + row);
        check(integration::journalOpenSet("tq-uniq-items-tg") && !integration::journalReadOnly(), "the file opens writable");
        check(integration::journalCount() == 2, "2 good rows kept, the broken and the mis-keyed line dropped");
        unsigned long long seq = 0;
        bool rb = false;
        check(integration::journalDepositCommit(cap(kRing, 2), &seq, &rb), "the next deposit writes");
        check(countBad("tq-uniq-items-tg") == 1, "the original was copied aside as .bad-<stamp> first");
    }

    printf("\nM2. a row that says \"stack\":0 is read as one copy (never dropped)\n");
    {
        clean("tq-uniq-items-tz");
        char p[MAX_PATH];
        pathOf("tq-uniq-items-tz", "jsonl", p);
        writeAll(p, "{\"journal\":\"titan quest uniquetab\",\"format\":1,\"entries\":1}\n"
                    "{\"record\":\"records/item/equipmentring/u_n_ring_01.dbr\",\"stack\":0,"
                    "\"base\":\"records\\\\item\\\\equipmentring\\\\u_n_ring_01.dbr\",\"prefix\":\"\","
                    "\"suffix\":\"\",\"relic\":\"\",\"relicBonus\":\"\",\"relic2\":\"\",\"relicBonus2\":\"\","
                    "\"seed\":1,\"var1\":0,\"var2\":0,\"b8\":0}\n");
        check(integration::journalOpenSet("tq-uniq-items-tz") && integration::journalRows(kRingKey) == 1 &&
                  integration::journalCount() == 1,
              "the stack-0 row is kept (utJournalStack: one copy)");
    }

    printf("\nN. the CSV export\n");
    {
        clean("tq-uniq-items-th");
        check(integration::journalOpenSet("tq-uniq-items-th"), "set th");
        integration::journalSetCsvExport(1);
        unsigned long long seq = 0;
        bool rb = false;
        check(integration::journalDepositCommit(cap(kRing, 42, "a,b"), &seq, &rb), "deposit");
        char c[MAX_PATH];
        pathOf("tq-uniq-items-th", "csv", c);
        const std::string t = readAll(c);
        check(t.find("record,collected,deposited,stack,base,") == 0, "the CSV header");
        check(t.find("records/item/equipmentring/u_n_ring_01.dbr,unsaved,") != std::string::npos,
              "one row, marked unsaved");
        check(t.find("\"a,b\"") != std::string::npos, "RFC 4180 quoting");
        integration::journalSetCsvExport(0);
    }

    printf("\nN. the load settles against a save bounded by the process start; "
           "a take held on the cursor is never settled\n");
    {
        const unsigned long long S = 1000000000ull;   // 100 s in FILETIME units
        check(!integration::utLoadSettles(10 * S, 0, 20 * S) && !integration::utLoadSettles(10 * S, 15 * S, 0) &&
                  !integration::utLoadSettles(10 * S, 9 * S, 20 * S),
              "utLoadSettles: no save / unknown start / an older save -> never");
        check(integration::utLoadSettles(10 * S, 15 * S, 20 * S), "an earlier process's row, saved before this start -> settled");
        check(!integration::utLoadSettles(10 * S, 25 * S, 20 * S), "an earlier process's row, a save after this start -> NOT settled");
        check(integration::utLoadSettles(21 * S, 25 * S, 20 * S), "this process's row, a newer save -> settled");

        clean("tq-uniq-items-ti");
        check(integration::journalOpenSet("tq-uniq-items-ti"), "set ti");
        bool rb = false;
        unsigned long long s = 0;
        check(integration::journalDepositCommit(cap(kRing, 7), &s, &rb) &&
                  integration::journalDepositCommit(cap(kRing, 8), &s, &rb) &&
                  integration::journalDepositCommit(cap(kRing, 9), &s, &rb),
              "three deposits");
        check(integration::journalSaveObserved(now() + 10) == 3, "a save settles them");
        integration::UtReplicaCapture n;
        unsigned long long seq = 0;

        // (a) an earlier process: take, a save after it, then this process starts
        integration::journalNewest(kRingKey, &n, &seq);
        check(integration::journalTakeCommit(kRingKey, seq, &rb) && integration::journalPendingOut() == 1, "(a) take a saved row");
        unsigned long long t = now();
        integration::journalFlushNow();
        check(integration::journalOpenSet("tq-uniq-items-tb") &&
                  integration::journalOpenSet("tq-uniq-items-ti", t + 20, t + 40),
              "(a) reload: the save (t+20) is older than this process (t+40)");
        check(integration::journalCount() == 2 && integration::journalRows(kRingKey) == 2 && integration::journalPendingOut() == 0,
              "(a) the take is SETTLED at load - no silent duplicate");

        // (b) an earlier process: take, crash, this process starts and saves before the load
        integration::journalNewest(kRingKey, &n, &seq);
        check(integration::journalTakeCommit(kRingKey, seq, &rb), "(b) take a saved row");
        t = now();
        integration::journalFlushNow();
        check(integration::journalOpenSet("tq-uniq-items-tb") &&
                  integration::journalOpenSet("tq-uniq-items-ti", t + 20, t + 10),
              "(b) reload: the only newer save (t+20) was written by THIS process (start t+10)");
        check(integration::journalCount() == 2 && integration::journalRows(kRingKey) == 2 && integration::journalPendingOut() == 0,
              "(b) the take is RESTORED - a duplicate at worst, never a loss");

        // (c) this process: take, world switch A -> B -> A, a save in between
        integration::journalNewest(kRingKey, &n, &seq);
        check(integration::journalTakeCommit(kRingKey, seq, &rb), "(c) take a saved row");
        t = now();
        integration::journalFlushNow();
        check(integration::journalOpenSet("tq-uniq-items-tb") &&
                  integration::journalOpenSet("tq-uniq-items-ti", t + 20, t - 100 * S),
              "(c) reload in the same process (started long before the take), a newer save");
        check(integration::journalCount() == 1 && integration::journalRows(kRingKey) == 1 && integration::journalPendingOut() == 0,
              "(c) this process's take is settled by its newer save");

        // a take held on the cursor is not settled; after it leaves, only a LATER save settles
        integration::journalNewest(kRingKey, &n, &seq);
        check(integration::journalTakeCommit(kRingKey, seq, &rb), "(d) take the last row");
        integration::journalSetHeldSeq(seq);
        check(integration::journalSaveObserved(now() + 10) == 0 && integration::journalPendingOut() == 1,
              "(d) a save while the item is on the cursor settles nothing");
        Sleep(30);
        const unsigned long long whileHeld = now();
        Sleep(30);
        integration::journalSetHeldSeq(0);
        check(integration::journalTouchOut(seq), "(d) the item left the cursor: the taken time moves to now");
        check(!integration::journalTouchOut(seq + 1000), "(d) no such out row -> false");
        check(integration::journalSaveObserved(whileHeld) == 0 && integration::journalPendingOut() == 1,
              "(d) a save written while it was held still settles nothing");
        check(integration::journalSaveObserved(now() + 10) == 1 && integration::journalCount() == 0,
              "(d) a save after it left the cursor settles the take");
        integration::journalFlushNow();
    }

    printf("\nW. the pending rows settled against the character's containers\n");
    {
        using namespace integration;
        const std::string::size_type npos = std::string::npos;
        // the pure verdict: the 2 x 3 table + unknown
        check(utPendingVerdict(1, kUtFoundPresent) == kUtVerdictErase,
              "in  + present        -> ERASE (the removal never persisted: the item is back)");
        check(utPendingVerdict(1, kUtFoundAbsent) == kUtVerdictSettle,
              "in  + absent         -> SETTLE (the removal persisted: collected)");
        check(utPendingVerdict(1, kUtFoundAbsentSoFar) == kUtVerdictKeep,
              "in  + absent so far  -> keep (the caravan sacks are not searched yet)");
        check(utPendingVerdict(2, kUtFoundPresent) == kUtVerdictErase,
              "out + present        -> ERASE (the take persisted: removed for good)");
        check(utPendingVerdict(2, kUtFoundAbsent) == kUtVerdictRestore,
              "out + absent         -> RESTORE (the take never persisted)");
        check(utPendingVerdict(2, kUtFoundAbsentSoFar) == kUtVerdictKeep,
              "out + absent so far  -> keep");
        check(utPendingVerdict(1, kUtFoundUnknown) == kUtVerdictKeep &&
                  utPendingVerdict(2, kUtFoundUnknown) == kUtVerdictKeep,
              "unknown              -> keep (settle nothing)");
        check(utPendingVerdict(0, kUtFoundPresent) == kUtVerdictKeep &&
                  utPendingVerdict(3, kUtFoundAbsent) == kUtVerdictKeep,
              "a row that is not pending -> keep");

        // the replica compare (the ten fields) on fixtures
        const UtReplicaCapture a =
            cap(kRing, 7, "records/item/lootmagicalaffixes/prefix/p1.dbr", "records/item/relics/r1.dbr");
        UtReplicaCapture b = a;
        check(utReplicaSame(a, b), "compare: the same replica -> a match");
        b.seed = 8;
        check(!utReplicaSame(a, b), "compare: the seed off by one -> no match");
        b = a;
        _snprintf_s(b.str[kUtIdSuffix], kUtIdStrMax, _TRUNCATE, "%s",
                    "records/item/lootmagicalaffixes/suffix/s2.dbr");
        check(!utReplicaSame(a, b), "compare: a different suffix -> no match");
        b = a;
        b.var2 = 1;
        check(!utReplicaSame(a, b), "compare: var2 differs -> no match");
        b = a;
        _snprintf_s(b.str[kUtIdRelic], kUtIdStrMax, _TRUNCATE, "%s", "records/item/relics/R1.dbr");
        check(!utReplicaSame(a, b), "compare: the relic differs in case only -> no match (byte-exact)");
        const UtReplicaCapture* two[2] = {&a, &a};
        int f2[2] = {0, 0};
        const int m0 = utMatchLive(two, f2, 2, a);
        if (m0 >= 0) f2[m0] = 1;
        const int m1 = utMatchLive(two, f2, 2, a);
        if (m1 >= 0) f2[m1] = 1;
        check(m0 == 0 && m1 == 1 && utMatchLive(two, f2, 2, a) == -1,
              "one live item proves ONE of two byte-identical rows");

        // the file: four verdicts + keep, in one deferred load
        const char* L = "tq-uniq-items-tw";
        clean(L);
        char wpath[MAX_PATH];
        pathOf(L, "jsonl", wpath);
        check(journalOpenSet(L), "set tw");
        bool rb = false;
        unsigned long long s = 0;
        for (unsigned k = 1; k <= 4; ++k) journalDepositCommit(cap(kRing, k), &s, &rb);
        check(journalSaveObserved(now() + 10) == 4, "four rows (seeds 1..4), saved");
        UtReplicaCapture nn;
        unsigned long long seq = 0;
        journalNewest(kRingKey, &nn, &seq);
        const bool t4 = nn.seed == 4 && journalTakeCommit(kRingKey, seq, &rb);
        journalNewest(kRingKey, &nn, &seq);
        const bool t3 = nn.seed == 3 && journalTakeCommit(kRingKey, seq, &rb);
        check(t4 && t3, "take seeds 4 and 3 (pending out)");
        for (unsigned k = 5; k <= 7; ++k) journalDepositCommit(cap(kRing, k), &s, &rb);
        check(journalPendingIn() == 3 && journalPendingOut() == 2 && journalCount() == 7,
              "deposit seeds 5, 6, 7 (pending in): 7 rows, 3 in, 2 out");
        journalFlushNow();
        check(journalOpenSet("tq-uniq-items-tb") && journalOpenSet(L, 0, 0, true),
              "the crash: a DEFERRED reload with no save seen");
        check(journalUnresolved() == 5 && journalPendingIn() == 3 && journalPendingOut() == 0 &&
                  journalRows(kRingKey) == 7,
              "5 unresolved; the 2 outs are RESTORED at the open, the 3 ins wait");
        check(journalSaveObserved(100) == 0 && journalUnresolved() == 5,
              "a save older than the rows leaves the unresolved rows alone");
        UtPendingRow pr[8];
        const size_t np = journalUnresolvedRows(pr, 8);
        check(np == 5, "the 5 rows are handed to the container check");
        UtVerdictItem v[8];
        unsigned long long seq7 = 0;
        for (size_t i = 0; i < np; ++i) {
            const unsigned sd = pr[i].id.seed;
            // found: 5 (in) and 4 (out) in the inventory; 7 not yet (no caravan); 6, 3 nowhere
            const int f = sd == 5 || sd == 4 ? kUtFoundPresent
                          : sd == 7          ? kUtFoundAbsentSoFar
                                             : kUtFoundAbsent;
            v[i].seq = pr[i].seq;
            v[i].verdict = utPendingVerdict(pr[i].state, f);
            v[i].where = f == kUtFoundPresent ? "inventory" : nullptr;
            if (sd == 7) seq7 = pr[i].seq;
        }
        const long w0 = journalWrites();
        check(journalApplyVerdicts(v, np) == 4,
              "apply: 5 erased, 4 erased, 6 settled, 3 restored (4 changed), 7 kept");
        check(journalWrites() == w0 + 1, "the journal is written ONCE for the settle");
        check(journalCount() == 5 && journalRows(kRingKey) == 5 && journalPendingIn() == 1 &&
                  journalPendingOut() == 0 && journalUnresolved() == 1,
              "after: 5 rows (1 2 3 6 7), 1 pending in (7, still unresolved), none out");
        std::string t = readAll(wpath);
        check(t.find("\"entries\":5,\"collected\":5,\"pendingIn\":1,\"pendingOut\":0") != npos &&
                  t.find("\"written\":\"") != npos,
              "on disk: the header says entries 5, collected 5, pendingIn 1, pendingOut 0, written");
        check(t.find("\"seed\":5,") == npos && t.find("\"seed\":4,") == npos &&
                  t.find("\"seed\":3,") != npos && t.find("\"seed\":6,") != npos &&
                  t.find("\"pending\":\"out\"") == npos,
              "on disk: 5 and 4 are gone, 3 (restored) and 6 (settled) are there, no out row");

        // unknown: keep, nothing written
        UtVerdictItem k1 = {seq7, utPendingVerdict(1, kUtFoundUnknown), nullptr};
        const long w1 = journalWrites();
        check(journalApplyVerdicts(&k1, 1) == 0 && journalWrites() == w1 && journalUnresolved() == 1,
              "unknown: settles nothing, no write");
        UtVerdictItem bad = {seq7, kUtVerdictRestore, nullptr};
        check(journalApplyVerdicts(&bad, 1) == 0 && journalPendingIn() == 1,
              "a verdict that does not fit the row's state (restore on an in row): nothing");
        // the fallback: today's rule
        check(journalUnresolvedFallback("test") == 1 && journalUnresolved() == 0 &&
                  journalPendingIn() == 1,
              "fallback: the in row is reported (WARN) and left pending, as today");
        check(journalSaveObserved(now() + 10) == 1 && journalPendingIn() == 0,
              "... and the next save settles it, as today");
        check(journalApplyVerdicts(v, np) == 0 && journalCount() == 5,
              "verdicts for rows that are no longer unresolved: nothing");
        journalNewest(kRingKey, &nn, &seq);
        check(journalTakeCommit(kRingKey, seq, &rb), "take the newest row (seed 7)");
        journalFlushNow();
        check(journalOpenSet("tq-uniq-items-tb") && journalOpenSet(L, 0, 0, true) &&
                  journalUnresolved() == 1 && journalRows(kRingKey) == 5,
              "a crash, a deferred reload: 1 unresolved out row, restored at the open");
        check(journalUnresolvedFallback("test") == 1 && journalRows(kRingKey) == 5 &&
                  journalPendingOut() == 0 && journalUnresolved() == 0,
              "fallback: the out row is RESTORED (today's rule, WARN)");
        journalFlushNow();
        t = readAll(wpath);
        check(t.find("\"entries\":5,\"collected\":5,\"pendingIn\":0,\"pendingOut\":0") != npos,
              "on disk: entries 5, collected 5, nothing pending");
    }

    printf("\nX. a deferred row meets a save of the next session (no caravan open)\n");
    {
        using namespace integration;
        // the repro: S1 takes, dies; S2 defers, saves, quits; S3 must keep the row
        const char* L = "tq-uniq-items-tx";
        clean(L);
        bool rb = false;
        unsigned long long s = 0;
        check(journalOpenSet(L), "S1: set tx");
        journalDepositCommit(cap(kRing, 11), &s, &rb);
        check(journalSaveObserved(now() + 10) == 1, "S1: seed 11 deposited and saved");
        UtReplicaCapture nn;
        unsigned long long seq = 0;
        journalNewest(kRingKey, &nn, &seq);
        check(journalTakeCommit(kRingKey, seq, &rb) && journalPendingOut() == 1,
              "S1: taken (pending out), then the crash");
        journalFlushNow();
        const unsigned long long p2 = now() + 1000;
        check(journalOpenSet("tq-uniq-items-tb") && journalOpenSet(L, 0, p2, true) &&
                  journalUnresolved() == 1 && journalPendingOut() == 0 && journalRows(kRingKey) == 1,
              "S2: deferred load - the out row is RESTORED at the open (unresolved)");
        journalFlushNow();   // the worker's write of the restore
        const unsigned long long save2 = p2 + 1000;
        check(journalSaveObserved(save2) == 0, "S2: a character save; no caravan open; exit");
        journalFlushNow();
        const unsigned long long p3 = save2 + 1000;
        check(journalOpenSet("tq-uniq-items-tb") && journalOpenSet(L, save2, p3, true),
              "S3: deferred load, newest save = S2's");
        printf("  S3 after open: count=%d rows=%d pendingOut=%d unresolved=%d\n",
               (int)journalCount(), (int)journalRows(kRingKey), (int)journalPendingOut(),
               (int)journalUnresolved());
        check(journalCount() == 1 && journalRows(kRingKey) == 1 && journalPendingOut() == 0 &&
                  journalUnresolved() == 0,
              "S3: the row survives, in the collection (never a loss)");

        // the in-row mirror: S1 deposits, dies; S2 defers, then a save of S2 settles with a WARN
        const char* Ly = "tq-uniq-items-ty";
        clean(Ly);
        check(journalOpenSet(Ly), "S1: set ty");
        journalDepositCommit(cap(kRing, 12), &s, &rb);
        journalDepositCommit(cap(kRing, 13), &s, &rb);
        journalFlushNow();
        const unsigned long long q2 = now() + 1000;
        check(journalOpenSet("tq-uniq-items-tb") && journalOpenSet(Ly, 0, q2, true) &&
                  journalUnresolved() == 2 && journalPendingIn() == 2,
              "S2: deferred load, 2 unresolved in rows");
        check(journalSaveObserved(q2 + 1000) == 2 && journalUnresolved() == 0 &&
                  journalPendingIn() == 0 && journalRows(kRingKey) == 2,
              "S2: a character save - today's WARN first, then the save settles both (as)");

        // The verdicts and the fallback of one caravan pass write the journal ONCE
        journalNewest(kRingKey, &nn, &seq);
        check(nn.seed == 13 && journalTakeCommit(kRingKey, seq, &rb), "take seed 13 (pending out)");
        journalDepositCommit(cap(kRing, 14), &s, &rb);
        journalFlushNow();
        check(journalOpenSet("tq-uniq-items-tb") && journalOpenSet(Ly, 0, 0, true) &&
                  journalUnresolved() == 2 && journalRows(kRingKey) == 3,
              "a crash, a deferred reload: 13 (out, restored) and 14 (in) unresolved");
        journalFlushNow();
        UtPendingRow py[4];
        const size_t ny = journalUnresolvedRows(py, 4);
        UtVerdictItem vy[4];
        for (size_t i = 0; i < ny; ++i) {
            vy[i].seq = py[i].seq;
            vy[i].verdict = py[i].id.seed == 13 ? utPendingVerdict(py[i].state, kUtFoundPresent)
                                                : utPendingVerdict(py[i].state, kUtFoundUnknown);
            vy[i].where = "inventory";
        }
        const long wy = journalWrites();
        check(ny == 2 && journalApplyVerdicts(vy, ny, "test") == 1 && journalWrites() == wy + 1,
              "13 found (erased), 14 unknown (the fallback): ONE write");
        check(journalUnresolved() == 0 && journalPendingIn() == 1 && journalPendingOut() == 0 &&
                  journalRows(kRingKey) == 2 && journalCount() == 2,
              "after: 12 and 14 (14 reported, left to the next save), 13 gone");
        const long wz = journalWrites();
        check(journalApplyVerdicts(nullptr, 0, "test") == 0 && journalWrites() == wz,
              "a fallback with nothing unresolved: no write");
    }

    printf("\nZ. a generated row (tools\\make_all_items.py): read, re-written byte for byte, taken\n");
    {
        using namespace integration;
        const char* stamp = "2026-01-02T03:04:05.0000000Z";
        const char* b1 = "records\\item\\equipmentring\\u_n_ring_01.dbr";
        const char* b2 = "records\\xpack\\item\\equipmentweapon\\sword\\u_e_blade's_edge.dbr";
        const std::string r1 = allItemsRow(b1, stamp), r2 = allItemsRow(b2, stamp);
        clean("tq-uniq-items-tz");
        char pz[MAX_PATH];
        pathOf("tq-uniq-items-tz", "jsonl", pz);
        writeAll(pz, std::string("{\"journal\":\"titan quest uniquetab\",\"format\":1,\"written\":\"") +
                         stamp + "\",\"set\":\"tq-uniq-items-tz\",\"entries\":2,\"collected\":2," +
                         "\"pendingIn\":0,\"pendingOut\":0}\n" + r1 + r2);
        check(journalOpenSet("tq-uniq-items-tz") && journalUsable() && journalCount() == 2 &&
                  journalCollectedTotal() == 2 && journalPendingIn() == 0 && journalPendingOut() == 0,
              "two generated rows: 2 rows, 2 collected, nothing pending, nothing dropped");
        const unsigned s1 = allItemsSeed(kRingKey);
        check(s1 >= 1u && s1 <= 32767u && allItemsSeed("records/a.dbr") != allItemsSeed("records/b.dbr"),
              "the generator's seed is in 1..32767 and follows the record");
        UtReplicaCapture row;
        check(takeRoundTrip(kRingKey, &row) && row.seed == s1 && row.var1 == 0 && row.var2 == 0 &&
                  row.b8 == 0 && strcmp(row.str[kUtIdBase], b1) == 0 && !row.str[kUtIdPrefix][0] &&
                  !row.str[kUtIdRelicBonus2][0],
              "the take's row -> replica -> row is exact (base, six empty strings, seed, var1/var2/b8 0)");
        UtReplica proto, fromRow;
        UtReplicaCapture pa, pb;
        // bytes +0x04..+0x07 are the long base's pointer (each replica's own buffer): its TEXT is
        // compared instead, through the identity read back out of both.
        check(utReplicaBuild(&proto, "records/item/equipmentring/u_n_ring_01.dbr") &&
                  utReplicaFromIdentity(&fromRow, row) && utReplicaReadIdentity(proto.bytes, &pa) &&
                  utReplicaReadIdentity(fromRow.bytes, &pb) && pa.seed == 0 && pb.seed == s1 &&
                  (pa.seed = s1, sameCap(pa, pb)) && memcmp(proto.bytes, fromRow.bytes, 4) == 0 &&
                  memcmp(proto.bytes + 8, fromRow.bytes + 8, kUtReplicaSeed - 8) == 0 &&
                  memcmp(proto.bytes + kUtReplicaSeed + 4, fromRow.bytes + kUtReplicaSeed + 4,
                         kUtReplicaSize - kUtReplicaSeed - 4) == 0,
              "its replica IS the page prototype's (utReplicaBuild) apart from the seed");
        unsigned long long sq = 0;
        bool rb = true;
        check(journalDepositCommit(cap("records\\item\\equipmentring\\u_n_ring_02.dbr", 9), &sq, &rb) &&
                  !rb,
              "a deposit rewrites the file");
        const std::string tz = readAll(pz);
        check(tz.find("\n" + r1) != std::string::npos && tz.find("\n" + r2) != std::string::npos,
              "the mod's writer re-emits both generated rows byte for byte (stamp included)");
        journalOpenSet(nullptr);
    }

    integration::journalOpenSet(nullptr);
    {
        using namespace integration;
        printf("\nMultiple rolled instances: read-only copy snapshots and newest withdrawal\n");
        clean("tq-uniq-items-copy-instances");
        check(journalOpenSet("tq-uniq-items-copy-instances"),"isolated copy-instance test set opened");
        UtReplicaCapture first=cap(kRing,123,"records/item/lootmagicalaffixes/prefix/first.dbr","records/item/relics/first.dbr");
        UtReplicaCapture second=cap(kRing,456,"records/item/lootmagicalaffixes/prefix/second.dbr","records/item/relics/second.dbr");
        for(int k=kUtIdSuffix;k<kUtIdStrCount;++k) {
            sprintf_s(first.str[k],"records/item/instance-a-%d.dbr",k);
            sprintf_s(second.str[k],"records/item/instance-b-%d.dbr",k);
        }
        first.var1=17;first.var2=29;first.b8=7;
        second.var1=37;second.var2=49;second.b8=255;
        unsigned long long a=0,b=0;bool rb=false;
        check(journalDepositCommit(first,&a,&rb) && journalDepositCommit(second,&b,&rb) && a!=b,"same base record stores two distinct fully rolled rows");
        const std::string disk=readAll(journalPath());const long writes=journalWrites();
        const size_t pendingIn=journalPendingIn(),pendingOut=journalPendingOut();
        bool exact=true;
        for(int i=0;i<50;++i) {
            UtReplicaCapture snapshot={},copy={};unsigned long long seq=0;UtReplica replica;
            if(!journalNewest(kRingKey,&snapshot,&seq) || seq!=b || !sameCap(snapshot,second) ||
               !utReplicaFromIdentity(&replica,snapshot) || !utReplicaReadIdentity(replica.bytes,&copy) || !sameCap(copy,second))exact=false;
            // Mutating the mod-owned reconstructed copy cannot mutate the stored original.
            copy.seed=0;copy.str[kUtIdPrefix][0]=0;
        }
        check(exact,"50 newest snapshots reproduce every saved field without mixing instances");
        check(journalRows(kRingKey)==2 && journalCount()==2 && journalWrites()==writes &&
              journalPendingIn()==pendingIn && journalPendingOut()==pendingOut && readAll(journalPath())==disk,
              "read-only reconstruction leaves counts, pending states, write count and journal bytes unchanged");
        check(!journalTakeCommit(kRingKey,a,&rb) && readAll(journalPath())==disk,"stale first-row withdrawal is refused without a write");
        check(journalTakeCommit(kRingKey,b,&rb),"ordinary withdrawal removes newest instance");
        UtReplicaCapture remaining={};unsigned long long seq=0;
        check(journalNewest(kRingKey,&remaining,&seq) && seq==a && sameCap(remaining,first),"older instance keeps its own seed, affixes, upgrades and fields");
        check(journalOpenSet("tq-uniq-items-copy-other") && journalOpenSet("tq-uniq-items-copy-instances") &&
              journalNewest(kRingKey,&remaining,&seq) && seq!=0 && sameCap(remaining,first),"remaining instance survives closing and reloading the journal (sequence is session-local)");
        journalOpenSet(nullptr);
    }
    integration::logShutdown();
    printf("\n%s - %d passed, %d failed\n", g_fail ? "JOURNAL TESTS FAILED" : "ALL PASS", g_pass,
           g_fail);
    return g_fail ? 1 : 0;
}
