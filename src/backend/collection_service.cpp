#include "backend/museum_state.h"
#include <cstring>

namespace museum {
namespace {
struct CategoryName { const char* label; const char* caption; Section section; };
constexpr CategoryName names[] = {
    {"Helms", "Helm", Section::Equipment}, {"Torso", "Torso", Section::Equipment},
    {"Arms", "Arms", Section::Equipment}, {"Legs", "Legs", Section::Equipment},
    {"Amulets", "Amulet", Section::Equipment}, {"Rings", "Ring", Section::Equipment},
    {"Shields", "Shield", Section::Weapons}, {"Axes", "Axe", Section::Weapons},
    {"Maces", "Mace", Section::Weapons}, {"Staves", "Staff", Section::Weapons},
    {"Swords", "Sword", Section::Weapons}, {"Throwing", "Throw", Section::Weapons},
    {"Spears", "Spear", Section::Weapons}, {"Bows", "Bow", Section::Weapons},
    {"Artifacts", "Artifact", Section::Other}
};
}
Section categorySection(const char* label) {
    for (const auto& n : names) if (label && std::strcmp(n.label, label) == 0) return n.section;
    return Section::Other;
}
const char* categoryCaption(const char* label) {
    for (const auto& n : names) if (label && std::strcmp(n.label, label) == 0) return n.caption;
    return label ? label : "?";
}
const char* sectionCaption(Section s) {
    return s == Section::Equipment ? "Equipment" : s == Section::Weapons ? "Weapons" : "Other";
}
MuseumState makeMuseumState(const CollectionSnapshot& source) {
    MuseumState state;
    static_cast<CollectionSnapshot&>(state) = source;
    if (state.categoryCount < 0) state.categoryCount = 0;
    if (state.categoryCount > maxCategories) state.categoryCount = maxCategories;
    if (state.visibleCount < 0) state.visibleCount = 0;
    if (state.visibleCount > maxVisibleItems) state.visibleCount = maxVisibleItems;
    for (int i = 0; i < state.categoryCount; ++i) {
        const auto& c = state.categories[i];
        const int section=static_cast<int>(c.section);
        if (source.searchText[0] && c.searchMatch && section>=0 && section<3)
            state.sectionSearchMatch[section]=true;
        if (c.id == state.selectedCategory) state.activeSection = c.section;
        if (c.id == state.shownCategory) state.shownCategoryName = c.caption;
    }
    for (int i = 0; i < state.categoryCount; ++i)
        if (state.categories[i].section == state.activeSection)
            state.sectionCategories[state.sectionCategoryCount++] = state.categories[i];
    return state;
}
} // namespace museum
