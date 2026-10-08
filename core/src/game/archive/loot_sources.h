#pragma once
#include "game/archive/catalogue_gen.h"
#include <memory>
#include <map>
namespace gen {
struct LootContext {
    int averageLevel=0,minLevel=0,maxLevel=0,players=0,difficulty=0;
    bool valid() const;
    bool operator==(const LootContext& other) const;
};
// Complete model is persisted alongside the catalogue. Calculation never reads game memory.
class LootSourceModel {
public:
    LootSourceModel();
    ~LootSourceModel();
    bool load(const std::string& path,std::string* error);
    bool calculate(const LootContext&,std::string& output,std::string* error,
                   std::map<std::string,std::string>* farmTargets=nullptr);
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
bool buildLootSources(const GameData&,const std::vector<CatalogueItem>&,
                      std::string& output,std::vector<std::string>& warnings,std::string* error,
                      std::string* modelOutput=nullptr);
}
