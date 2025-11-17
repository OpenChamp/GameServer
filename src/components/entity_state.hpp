#pragma once

#include "component.hpp"

/**
 * Entity state enumeration.
 * Defines all possible states an entity can be in during its lifecycle.
 */
enum class EntityState {
    SPAWNED,
    IDLE,
    PATHFINDING_WAITING,
    MOVING,
    STUCK,
    ATTACKING,
    DEAD
};

/**
 * EntityState component for state machine management.
 * Tracks the current state of an entity and enables state-based behaviors.
 */
struct EntityStateComponent : public Component {
    
    EntityState current_state = EntityState::SPAWNED;
    float state_duration_ms = 0.0f;
    
    COMPONENT_TYPE_ID(EntityStateComponent, 2010)
};
