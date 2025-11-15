#pragma once

#include "component.hpp"
#include "../systems/math.hpp"
#include <vector>

/**
 * Movement component for entities.
 * Handles position, velocity, and pathfinding.
 */

 struct PathfindingComponent : public Component {
    // Stores only the pathfinding data
    std::vector<Vec2> path;
    int current_waypoint_index = 0;
    COMPONENT_TYPE_ID(PathfindingComponent, 2009)
 };