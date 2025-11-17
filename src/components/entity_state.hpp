#pragma once

#include "component.hpp"
#include <systems/entity_manager.hpp>

/**
 * Entity state enumeration.
 * Defines all possible states an entity can be in during its lifecycle.
 */
enum class EntityState {
    SPAWNED,
    IDLE,
    // Idle is for players || If jungle is item and is attacked, it goes from SPAWNED -> IDLE -> ATTACKING
    PATHFINDING_WAITING,
    HOLDING_FOR_TARGET,
    MOVING,
    STOPPING,
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
    EntityID target_entity_id = INVALID_ENTITY_ID;
    std::vector<Vec2> target_positions = {};

    float state_duration_ms = 0.0f;
    
    COMPONENT_TYPE_ID(EntityStateComponent, 2010)
};
