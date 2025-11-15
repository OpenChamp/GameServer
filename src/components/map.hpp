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
    
    // Navmesh
    std::vector<Vec2> vertices;
    std::vector<std::vector<uint32_t>> polygons;
    
    // Grid size and offset
    Vec2 size = Vec2(0.0f, 0.0f); //width, height
    Vec2 offset = Vec2(0.0f, 0.0f); // 1/2 width, 1/2 height - cmkrist 15/11/2025
    
    COMPONENT_TYPE_ID(Map, 2005)
};
