#pragma once

#include "component.hpp"
#include "../systems/math.hpp"
#include <vector>
#include <cstdint>

/**
 * Pathfinding component for entities.
 * Stores path waypoints and tracks progress along the path.
 */
struct PathfindingComponent : public Component {
    std::vector<Vec3> waypoints;
    int current_waypoint_index = 0;
    uint32_t target_spawnpoint_id = 0;
    
    bool is_waiting_for_path = false;
    // Request ID for tracking async pathfinding requests
    uint32_t path_request_id = 0;
    // Track last position to detect if entity is stuck
    Vec2 last_position = Vec2(0.0f, 0.0f);
    float stuck_time_ms = 0.0f;
    // Stuck Timeout Threshold
    static constexpr float STUCK_THRESHOLD_MS = 2000.0f;
    
    COMPONENT_TYPE_ID(PathfindingComponent, 2009)
};