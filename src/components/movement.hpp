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
    
    // Movement properties (passed from stats component when stats component is updated)
    float move_speed = 5.0f;
    
    // Pathfinding
    std::vector<Vec2> path;
    int current_waypoint_index = 0;
    
    COMPONENT_TYPE_ID(Movement, 2001)
};
