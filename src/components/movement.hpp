#pragma once

#include "component.hpp"
#include "../systems/math.hpp"
#include <vector>

/**
 * Movement component for entities.
 * Handles position, velocity, and collision (physicality) -- cmkrist 15/11/2025
 */
struct Movement : public Component {
    // Position (Rotation is client-side for now)
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    
    // Collision radius for entity-to-entity collision avoidance
    float collision_radius{0.5f}; 
    
    COMPONENT_TYPE_ID(Movement, 2001)
};