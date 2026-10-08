#include "game/catalogue_generation.h"
#include "game/core_client.h"
#include "core/logging.h"
namespace integration {
void generateEnsure(HMODULE){char folder[MAX_PATH]={};const auto* api=coreApi();if(api&&api->copyDataDirectory(folder,MAX_PATH))logI("catalogue: consuming shared core data at %s",folder);}
bool generateBusy(){return false;}
bool generateDone(){const auto* api=coreApi();return api&&api->isReady();}
}
