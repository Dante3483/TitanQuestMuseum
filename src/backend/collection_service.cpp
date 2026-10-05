#include "backend/localization.h"
#include "backend/museum_state.h"
#include <cstring>

namespace museum {
namespace {
struct CategoryName { const char* label; const char* caption; Section section; };
constexpr CategoryName names[] = {
    {"Helms", "museum.category.helm", Section::Equipment}, {"Torso", "museum.category.torso", Section::Equipment},
    {"Arms", "museum.category.arms", Section::Equipment}, {"Legs", "museum.category.legs", Section::Equipment},
    {"Amulets", "museum.category.amulet", Section::Equipment}, {"Rings", "museum.category.ring", Section::Equipment},
    {"Shields", "museum.category.shield", Section::Weapons}, {"Axes", "museum.category.axe", Section::Weapons},
    {"Maces", "museum.category.mace", Section::Weapons}, {"Staves", "museum.category.staff", Section::Weapons},
    {"Swords", "museum.category.sword", Section::Weapons}, {"Throwing", "museum.category.throw", Section::Weapons},
    {"Spears", "museum.category.spear", Section::Weapons}, {"Bows", "museum.category.bow", Section::Weapons},
    {"Artifacts", "museum.category.artifact", Section::Other}
};
}
Section categorySection(const char* label) {
    for (const auto& n : names) if (label && std::strcmp(n.label, label) == 0) return n.section;
    return Section::Other;
}
const char* categoryCaption(const char* label) {
    for (const auto& n : names) if (label && std::strcmp(n.label, label) == 0) return i18n::text(n.caption);
    return label ? label : "?";
}
const char* sectionCaption(Section s) {
    return i18n::text(s == Section::Equipment ? "museum.section.equipment" : s == Section::Weapons ? "museum.section.weapons" : "museum.section.other");
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
