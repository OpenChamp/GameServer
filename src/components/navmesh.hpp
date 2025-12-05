#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <vector>

/**
 * NavMesh component for map entities.
 * Stores the navigation mesh dimensions and spawnpoint locations.
 */
struct NavMesh : public Component {
    // Navmesh dimensions
    float width = 100.0f;  // X-axis extent
    float height = 20.0f;  // Z-axis extent
    float y_level = 0.0f;  // Y position of the navmesh surface
    
    // Spawnpoints (typically at opposite ends of the navmesh)
    std::vector<Vec3> spawnpoints;
    
    COMPONENT_TYPE_ID(NavMesh, 2003)
};
