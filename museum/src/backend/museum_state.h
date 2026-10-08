#pragma once
#include <cstddef>

namespace museum {
enum class Section { Equipment, Weapons, Other, Sets };
enum class Mode { Transfer, Collection };
enum class ActionKind { None, SelectSection, SelectCategory, ShowTransfer, ShowCollection,
                        ToggleOwned, FocusSearch, ClearSearch, BackToSets, ScrollSets };
struct Action { ActionKind kind = ActionKind::None; int value = 0; };
constexpr int setsPerSheet = 16;
constexpr int maxCategories = 160;
constexpr int maxVisibleItems = 240;
constexpr int maxQueryLength = 32;
struct Counts { int owned = 0, total = 0; bool known = false; };
struct Category {
    int id = -1;
    Section section = Section::Other;
    const char* caption = "";
    int total = 0;
    bool searchMatch = false;
    bool indexed = false;
    int discovered = 0;
};
struct VisibleItem { const char* record = ""; bool discovered = false, searchMatch = false; };
// Adapter snapshot: no engine pointers, coordinates or fonts cross this boundary.
struct CollectionSnapshot {
    bool setsList = false;
    int setsOffset = 0;
    Mode mode = Mode::Transfer;
    int selectedCategory = 0, shownCategory = 0;
    bool ownedOnly = false, searchEnabled = false, searchFocused = false;
    wchar_t searchText[maxQueryLength + 1] = {};
    Category categories[maxCategories];
    int categoryCount = 0;
    Counts categoryCounts, allCounts;
    VisibleItem visibleItems[maxVisibleItems];
    int visibleCount = 0;
    char searchStatus[40] = {};
};
struct MuseumState : CollectionSnapshot {
    bool sectionSearchMatch[4] = {};
    Section activeSection = Section::Equipment;
    Category sectionCategories[maxCategories];
    int sectionCategoryCount = 0;
    const char* shownCategoryName = "";
};
Section categorySection(const char* catalogueLabel);
const char* categoryCaption(const char* catalogueLabel);
const char* sectionCaption(Section section);
MuseumState makeMuseumState(const CollectionSnapshot& source);
} // namespace museum
