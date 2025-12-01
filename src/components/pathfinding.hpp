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

    // Request ID for tracking async pathfinding requests
    uint32_t path_request_id = 0;

    // True if the entity has requested a path and is currently waiting for it to be calculated
    bool waiting_for_path;

    COMPONENT_TYPE_ID(PathfindingComponent, 2009)
};