#pragma once
#include "backend/museum_state.h"
namespace museum::game {
bool initializeCollection(void* selfModule);
CollectionSnapshot readCollection();
void apply(Action action);
void blurSearch();
}
