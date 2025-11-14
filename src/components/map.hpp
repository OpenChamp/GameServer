#pragma once

#include "component.hpp"
#include "../systems/math.hpp"
#include <string>
#include <vector>

/**
 * Map component for the map entity.
 * Stores map metadata including name and navmesh data.
 */
struct Map : public Component {
    // Map name (e.g., "Konda")
    std::string name = "DEBUG_MAP";
    
    // Navmesh vertices
    std::vector<Vec3> vertices;
    
    // Navmesh polygons (triangle indices)
    std::vector<std::vector<uint32_t>> polygons;
    
    COMPONENT_TYPE_ID(Map, 2005)
};
