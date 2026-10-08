#pragma once
#include <windows.h>
namespace integration {
// Core generates and owns the shared outputs before Museum initializes.
void generateEnsure(HMODULE);
bool generateBusy();
bool generateDone();
}
