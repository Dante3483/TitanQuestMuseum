#pragma once
#include "tqt_radar.h"
#include <set>
#include <string>
namespace integration {
void startLootInspector(const TqtMobRadarApiV2*);
void setLootCollection(const std::set<std::string>&,bool known);
}
