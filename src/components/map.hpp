#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <string>
#include <vector>

/**
 * Represents a single structure on the map (tower, core, etc.) (may become a full component later) -- cmkrist 4/12/2025
 */
struct MapStructure {
    std::string id;
    std::string type;
    uint32_t team = 0;
    Vec3 position;
    Vec3 rotation;
    Vec3 scale;
};

/**
 * Map component for the map entity.
 * Stores map metadata including name, navmesh data, spawnpoint information, and structures.
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
    
    // Structures on the map (towers, cores, etc.)
    std::vector<MapStructure> structures;
    
    // Grid size and offset
    Vec2 size = Vec2(0.0f, 0.0f); //width, height
    Vec2 offset = Vec2(0.0f, 0.0f); // 1/2 width, 1/2 height - cmkrist 15/11/2025
    
    // ===== Spatial Grid (computed at load time) =====
    // 2D grid of polygon IDs for fast point-in-polygon queries.
    // Grid layout: grid_cells[y * grid_width + x] = {polygon_ids}
    // Computed once at map load, never modified afterwards.
    
    std::vector<std::vector<uint32_t>> grid_cells;  // grid_cells[index] = list of polygon IDs
    Vec2 grid_origin;                                // World position of grid origin (bottom-left)
    float grid_cell_size = 50.0f;                    // Size of each grid cell in world units
    int grid_width = 0;                              // Number of cells in X direction
    int grid_height = 0;                             // Number of cells in Y direction
    
    COMPONENT_TYPE_ID(Map, 2005)
};
