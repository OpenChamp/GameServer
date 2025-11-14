#pragma once

#include "component.hpp"
#include "../systems/math.hpp"
#include <vector>

/**
 * Movement component for entities.
 * Handles position, velocity, and pathfinding.
 */
struct Movement : public Component {
    // Position and rotation
    Vec3 position{0.0f, 0.0f, 0.0f};
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    float rotation = 0.0f;
    
    // Movement properties
    float move_speed = 5.0f;
    bool is_moving = false;
    
    // Pathfinding
    std::vector<Vec3> path;
    int current_waypoint_index = 0;
    
    COMPONENT_TYPE_ID(Movement, 2001)
};
