#pragma once

#include "component.hpp"

/**
 * Minion component for enemy units.
 * Tracks minion-specific properties like target spawnpoint and state.
 */
struct Minion : public Component {
    // Target spawnpoint index (0 or 1, opposite of where the minion spawned)
    int target_spawnpoint = 0;
    
    // Whether this minion has reached its destination
    bool has_reached_destination = false;
    
    // Whether this minion is dead
    bool is_dead = false;
    
    COMPONENT_TYPE_ID(Minion, 2004)
};
