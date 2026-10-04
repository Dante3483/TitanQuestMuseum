#pragma once
#include "backend/museum_state.h"
namespace museum {
class MuseumController {
public:
    void refresh();
    const MuseumState& state() const { return state_; }
    void dispatch(Action action);
private:
    MuseumState state_;
};
}
