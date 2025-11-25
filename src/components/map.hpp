#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <string>
#include <vector>

/**
 * Map component for the map entity.
 * Stores map metadata including name, navmesh data, and spawnpoint information.
 */
struct Map : public Component {
    // Map name (e.g., "Konda")
    std::string name = "DEBUG_MAP";
    
    // Navmesh geometry
    std::vector<Vec2> vertices;
    std::vector<std::vector<uint32_t>> polygons;
    
    // Spawnpoint positions (used for minion waves)
    // Minions path from spawnpoint[i] to spawnpoint[(i+1) % spawnpoints.size()]
    std::vector<Vec2> spawnpoints;
    
    // Grid size and offset
    Vec2 size = Vec2(0.0f, 0.0f); //width, height
    Vec2 offset = Vec2(0.0f, 0.0f); // 1/2 width, 1/2 height - cmkrist 15/11/2025
    
    COMPONENT_TYPE_ID(Map, 2005)
};
