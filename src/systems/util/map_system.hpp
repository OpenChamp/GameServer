#pragma once

#include "entity_manager.hpp"
#include "math.hpp"
#include "packet_validator.hpp"
#include <optional>
#include <string>
#include <vector>
#include <cstring>

#include <components/map.hpp>

enum spawn_type : uint8_t {
        PLAYER_SPAWN,
        MINION_SPAWN,
        MONSTER_SPAWN,
        CAMP_SPAWN,
        OBJECTIVE_SPAWN
};

const std::unordered_map<std::string, spawn_type> spawn_groups = {
    {"player_spawn", spawn_type::PLAYER_SPAWN},
    {"minion_spawn", spawn_type::MINION_SPAWN},
    {"monster_spawn", spawn_type::MONSTER_SPAWN},
    {"camp_spawn", spawn_type::CAMP_SPAWN},
    {"objective_spawn", spawn_type::OBJECTIVE_SPAWN}
};

const std::unordered_map<std::string, std::int8_t> spawn_teams = {
    {"team1", 1},
    {"team2", 2}
};
/**
 * System to load and manage game maps from Godot tscn files.
 * Parses navmesh data and provides it to clients.
 */
class MapSystem {
public:
    /**
     * Data structure for navmesh information.
     */
    struct NavMeshData {
        std::vector<Vec2> vertices;
        std::vector<std::vector<uint32_t>> polygons;
    };

    struct SpawnPoint {
        Vec2 position;
        uint8_t team_id;
        spawn_type type;
    };
    
    /**
     * Load a map from a Godot tscn file.
     * If no path is provided or the file doesn't exist, scans the current directory for .tscn files.
     * If no map file is found using either method, returns nullptr.
     * @param map_name Name of the map (e.g., "Konda"). If empty, derived from filename.
     * @param file_path Path to the .tscn file. If empty, scans current directory.
     * @param entity_manager Reference to the entity manager
     * @return Pointer to the created map entity, or nullptr if loading failed
     */
    static std::optional<Map> load_map(const std::optional<std::string>& file_path);

    /**
     * Serialize map data to an ENet packet.
     * Packet format:
     *   [0]     - PACKET_TYPE::GAME_STATE
     *   [1-4]   - name length (uint32_t, little-endian)
     *   [5+N]   - map name (string)
     *   [5+N+0-3]   - vertex count (uint32_t)
     *   [5+N+4+...]  - vertices (3 floats each)
     *   [...-3]  - polygon count (uint32_t)
     *   [...]    - polygons (variable length)
     * 
     * @param map_entity The map entity to serialize
     * @return Vector with serialized map data, or empty vector on failure
     */
    static std::vector<uint8_t> serialize_map(Entity* map_entity);
private:
    /**
     * Structure to hold transformation matrix data.
     */
    struct TransformData {
        float matrix[3][3];
        Vec2 position;
        bool has_rotation;
    };
    
    /**
     * Structure to hold map bounds information (min/max coordinates).
     */
    struct MapBounds {
        Vec2 min;  // Minimum (x, y) coordinates
        Vec2 max;  // Maximum (x, y) coordinates
    };
    
    /**
     * Extract the Transform3D from the NavigationRegion3D node and apply it to navmesh vertices.
     * @param file_content The TSCN file content
     * @param vertices The vertices to transform (modified in-place)
     * @param spawnpoints The spawnpoints to transform (modified in-place)
     */
    static void apply_navigation_region_transform(const std::string& file_content, 
                                                   std::vector<Vec2>& vertices,
                                                   std::vector<Vec2>& spawnpoints);
    
    /**
     * Parse a Transform3D matrix string from Godot format.
     * Format: Transform3D(m00, m01, m02, m10, m11, m12, m20, m21, m22, x, y, z)
     * @param transform_str The transform string to parse
     * @return TransformData containing rotation matrix and position
     */
    static TransformData parse_transform_3d(const std::string& transform_str);
    
    /**
     * Parse a Godot tscn file and extract navmesh data.
     * Looks for NavigationMesh subsection with vertices and polygons.
     * @param file_content The contents of the tscn file
     * @return NavMeshData with vertices and polygons, empty if parsing failed
     */
    static NavMeshData parse_navmesh_from_tscn(const std::string& file_content);
    static std::vector<SpawnPoint> parse_spawnpoints_from_tscn(const std::string& file_content);
    static std::vector<Vec2> parse_vertices_to_2D(const std::string& vertices_str);
    static std::vector<std::vector<uint32_t>> parse_polygons(const std::string& polygons_str);
    static Vec2 calculate_size_from_vertices(const std::vector<Vec2>& vertices);
    static MapBounds calculate_bounds_from_vertices(const std::vector<Vec2>& vertices);
};
