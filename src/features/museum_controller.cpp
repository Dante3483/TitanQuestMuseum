#include "features/museum_controller.h"
#include "game/collection_bridge.h"
namespace museum {
void MuseumController::refresh() { state_ = makeMuseumState(game::readCollection()); }
void MuseumController::dispatch(Action action) { game::apply(action); }
}
