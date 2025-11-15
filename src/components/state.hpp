#pragma once

#include "component.hpp"
#include "../systems/math.hpp"

/**
 * Component State Machine for entities.
 * Manages the current state of an entity (e.g., idle, moving, attacking).
 */

enum class EntityState {
    IDLE,
    MOVING,
    ATTACKING,
    CASTING,
    STUNNED,
    DEAD
};

struct State : public Component {
    EntityState current_state = EntityState::IDLE;
    COMPONENT_TYPE_ID(State, 2000)
};