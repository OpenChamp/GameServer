#pragma once

#include "component.hpp"
#include "../systems/math.hpp"
#include <vector>

/**
 * Movement component for entities.
 * Handles position, velocity, and pathfinding.
 */
struct Movement : public Component {
    // Position (Rotation is client-side for now)
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 velocity{0.0f, 0.0f, 0.0f};

    // felt cute, might delete later (in favor of using a stats component instead) -- cmkrist 15/11/2025
    float move_speed{5.0f}; 
    
    COMPONENT_TYPE_ID(Movement, 2001)
};