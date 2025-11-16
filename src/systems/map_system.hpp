#pragma once

#include "entity_manager.hpp"
#include "math.hpp"
#include "packet_validator.hpp"
#include <optional>
#include <string>
#include <vector>
#include <cstring>

#include <components/map.hpp>
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
     * Parse a Godot tscn file and extract navmesh data.
     * Looks for NavigationMesh subsection with vertices and polygons.
     * @param file_content The contents of the tscn file
     * @return NavMeshData with vertices and polygons, empty if parsing failed
     */
    static NavMeshData parse_navmesh_from_tscn(const std::string& file_content);
    static std::vector<Vec2> parse_vertices_to_2D(const std::string& vertices_str);
    static std::vector<std::vector<uint32_t>> parse_polygons(const std::string& polygons_str);
    static Vec2 calculate_size_from_vertices(const std::vector<Vec2>& vertices);
};
