#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <vector>
#include <cstdint>

/**
 * Pathfinding component for entities.
 * Stores path waypoints and tracks progress along the path.
 * Entity state is tracked in EntityStateComponent (PATHFINDING_WAITING, MOVING, STUCK).
 */
struct PathfindingComponent : public Component {
    // Current path waypoints (2D positions)
    std::vector<Vec2> waypoints;
    
    // Index of the next waypoint to move toward
    int current_waypoint_index = 0;
    
    // Target spawnpoint ID (the destination spawnpoint this path leads to)
    uint32_t target_spawnpoint_id = 0;
    
    // Request ID for tracking async pathfinding requests
    uint32_t path_request_id = 0;
    
    // Track last position to detect if entity is stuck
    Vec2 last_position = Vec2(0.0f, 0.0f);
    
    // Time spent stuck without progress (milliseconds)
    float stuck_time_ms = 0.0f;
    
    // Threshold for considering entity stuck (milliseconds)
    static constexpr float STUCK_THRESHOLD_MS = 2000.0f;
    
    // Counter for failed pathfinding attempts (to prevent infinite retries)
    uint32_t pathfinding_retry_count = 0;
    static constexpr uint32_t MAX_PATHFINDING_RETRIES = 3;
    
    COMPONENT_TYPE_ID(PathfindingComponent, 2009)
};