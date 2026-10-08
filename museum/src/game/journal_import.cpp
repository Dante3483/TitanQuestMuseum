#include "game/journal_import.h"
#include "core/paths.h"
#include "core/logging.h"
#include <cwchar>
#include <cstdio>
namespace museum::game {
namespace {
bool siblingReference(const wchar_t* path,wchar_t* out) {
    std::wcsncpy(out,path,MAX_PATH-1); out[MAX_PATH-1]=0;
    wchar_t* end=std::wcsrchr(out,L'\\');
    if (!end) return false;
    *end=0;
    return wcscat_s(out,MAX_PATH,L"\\uniquetab")==0;
}
}
int copyMissingJournals(const wchar_t* reference,const wchar_t* museum) {
    if (!reference || !museum || _wcsicmp(reference,museum)==0) return 0;
    wchar_t pattern[MAX_PATH];
    if (swprintf_s(pattern,L"%s\\tq-uniq-items*.jsonl",reference)<0) return 0;
    WIN32_FIND_DATAW data;
    HANDLE search=FindFirstFileW(pattern,&data);
    if (search==INVALID_HANDLE_VALUE) return 0;
    int copied=0;
    do {
        if (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
        wchar_t from[MAX_PATH],to[MAX_PATH],temporary[MAX_PATH];
        if (swprintf_s(from,L"%s\\%s",reference,data.cFileName)<0 ||
            swprintf_s(to,L"%s\\%s",museum,data.cFileName)<0 ||
            swprintf_s(temporary,L"%s.import-%lu-%lu.tmp",to,GetCurrentProcessId(),GetTickCount())<0) continue;
        if (GetFileAttributesW(to)!=INVALID_FILE_ATTRIBUTES) continue;
        // Stage a private copy, then rename without replacement. A read/copy failure cannot
        // publish a truncated journal; an existing Museum journal always wins.
        if (!CopyFileW(from,temporary,TRUE)) continue;
        if (MoveFileW(temporary,to)) {
            ++copied;
            integration::logI("museum: imported reference journal %S (source retained)",data.cFileName);
        } else DeleteFileW(temporary);
    } while (FindNextFileW(search,&data));
    FindClose(search);
    return copied;
}
void importReferenceJournals(HMODULE module) {
    wchar_t museum[MAX_PATH],reference[MAX_PATH],dll[MAX_PATH];
    integration::utModPathW(module,L"",museum,MAX_PATH);
    if (!*museum) return;
    // utModPathW adds a separator even for an empty leaf; remove it for the sibling lookup.
    const size_t length=std::wcslen(museum);
    if (length && museum[length-1]==L'\\') museum[length-1]=0;
    if (siblingReference(museum,reference)) copyMissingJournals(reference,museum);
    if (GetModuleFileNameW(module,dll,MAX_PATH) && siblingReference(dll,reference))
        copyMissingJournals(reference,museum);
}
}
