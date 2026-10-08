#pragma once
#include <windows.h>
namespace museum::game {
// Copy missing format-compatible journals; never overwrite either collection.
int copyMissingJournals(const wchar_t* referenceDirectory,const wchar_t* museumDirectory);
void importReferenceJournals(HMODULE selfModule);
}
