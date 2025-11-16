#include "map_system.hpp"
#include "components/map.hpp"
#include "components/navmesh.hpp"
#include <log.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <vector>


std::optional<Map> MapSystem::load_map(const std::optional<std::string>& file_path) {
    std::string default_map_dir = "./data/maps/"; // STATIC -- cmkrist 15/11/2025
    // Check for default map directory, if missing error out and die
    if (!std::filesystem::exists(default_map_dir)) {
        LOG_ERROR("Default map directory does not exist: %s", default_map_dir.c_str());
        return std::nullopt;
    }
    // Create initial return variables
    std::optional<Map> loaded_map;
    std::string actual_file_path;
    // Check for existing file path
    if (file_path && !file_path->empty()) {
        // Dedicated Path
        if (file_path->find(".tscn") != std::string::npos) {
            LOG_INFO("Loading map from specified file path: %s", file_path->c_str());
            actual_file_path = *file_path;
            if (!std::filesystem::exists(actual_file_path)) {
                LOG_ERROR("Specified map file does not exist: %s", actual_file_path.c_str());
                actual_file_path.clear();
            }
        // Map Name
        } else {
            actual_file_path = default_map_dir + *file_path + ".tscn";
            if (std::filesystem::exists(actual_file_path)) {
                LOG_INFO("Loading map from specified map name: %s", file_path->c_str());
            } else {
                LOG_ERROR("Map file for specified map name does not exist: %s", actual_file_path.c_str());
                actual_file_path.clear();
            }
        }
    }
    // ScanDir if no map set
    if (actual_file_path.empty()) {
        // Check for valid file paths
        std::vector<std::string> tscn_files;
        for (const auto& entry : std::filesystem::directory_iterator(default_map_dir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".tscn") {
                tscn_files.push_back(entry.path().filename().string());
            }
        }
        
        if (tscn_files.empty()) {
            LOG_ERROR("No .tscn files found in current directory. Cannot load map.");
            return std::nullopt;
        }

        actual_file_path = default_map_dir + tscn_files[0];
        LOG_INFO("Found .tscn file: %s", actual_file_path.c_str());
    }
    // Read file
    std::ifstream file(actual_file_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open map file: %s", actual_file_path.c_str());
        return std::nullopt;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();
    file.close();
    
    // Parse navmesh
    NavMeshData navmesh_data = parse_navmesh_from_tscn(file_content);
    if (navmesh_data.vertices.empty() || navmesh_data.polygons.empty()) {
        LOG_ERROR("Failed to parse navmesh data from map file: %s", actual_file_path.c_str());
        return std::nullopt;
    }

    // Parse spawnpoints (not used yet) -- cmkrist 16/11/2025
    std::vector<SpawnPoint> spawnpoints = parse_spawnpoints_from_tscn(file_content);
    LOG_INFO("Parsed %zu spawnpoints from map file", spawnpoints.size());
    
    // Add Map component
    Map map;
    size_t final_slash_index = actual_file_path.find_last_of("/\\");
    // Get name from file
    map.name = actual_file_path.substr(final_slash_index + 1, actual_file_path.find_last_of('.') - final_slash_index - 1);
    map.vertices = navmesh_data.vertices;
    map.polygons = navmesh_data.polygons;
    // Get 2d size and offset
    map.size = MapSystem::calculate_size_from_vertices(navmesh_data.vertices);
    map.offset = map.size / 2.0f;

    LOG_INFO("Loaded map '%s' with %zu vertices and %zu polygons",
             map.name.c_str(), navmesh_data.vertices.size(), navmesh_data.polygons.size());

    return std::optional<Map>(map);
}

MapSystem::NavMeshData MapSystem::parse_navmesh_from_tscn(const std::string& file_content) {
    NavMeshData data;
    
    // Find the NavigationMesh subsection
    size_t nav_mesh_pos = file_content.find("[sub_resource type=\"NavigationMesh\"");
    if (nav_mesh_pos == std::string::npos) {
        LOG_WARN("NavigationMesh subsection not found in tscn file");
        return data;
    }
    
    // Find vertices line
    std::string vertices_string = "vertices = PackedVector3Array(";
    size_t vertices_pos = file_content.find(vertices_string, nav_mesh_pos);
    if (vertices_pos == std::string::npos) {
        LOG_WARN("vertices line not found in NavigationMesh");
        return data;
    }
    vertices_pos += vertices_string.length(); // Skip "vertices = PackedVector3Array("
    size_t vertices_end = file_content.find(")", vertices_pos);
    if (vertices_end == std::string::npos) {
        LOG_WARN("vertices end not found");
        return data;
    }
    
    std::string vertices_str = file_content.substr(vertices_pos, vertices_end - vertices_pos);
    data.vertices = parse_vertices_to_2D(vertices_str);

    // Find polygons line
    std::string polygons_string = "polygons = [";
    size_t polygons_pos = file_content.find(polygons_string, nav_mesh_pos);
    if (polygons_pos == std::string::npos) {
        LOG_WARN("polygons line not found in NavigationMesh");
        return data;
    }

    polygons_pos += polygons_string.length(); // Skip "polygons = ["
    size_t polygons_end = file_content.find("]", polygons_pos);
    if (polygons_end == std::string::npos) {
        LOG_WARN("polygons end not found");
        return data;
    }
    
    std::string polygons_data_raw = file_content.substr(polygons_pos, polygons_end - polygons_pos);
    data.polygons = parse_polygons(polygons_data_raw);
    
    return data;
}

std::vector<MapSystem::SpawnPoint> MapSystem::parse_spawnpoints_from_tscn(const std::string& file_content) {
    std::vector<SpawnPoint> spawnpoints;
    
    // Find all SpawnPoint nodes
    size_t pos = 0;
    while (true) {
        
        size_t spawn_pos = file_content.find("[node name=\"", pos);
        if (spawn_pos == std::string::npos) break;

        // Check type
        size_t type_pos = file_content.find("type=\"Marker3D\"", spawn_pos);
        if (type_pos == std::string::npos || type_pos > file_content.find("\n", spawn_pos)) {
            pos = spawn_pos + 1;
            continue; // Not a Marker3D node
        }

        // Check groups for team and spawn type
        size_t groups_pos = file_content.find("groups = [", spawn_pos);
        if (groups_pos == std::string::npos || groups_pos > file_content.find("\n", spawn_pos)) {
            pos = spawn_pos + 1;
            continue; // No groups found
        }
        size_t groups_end = file_content.find("]", groups_pos);
        
        // Create spawnpoint before parsing details
        SpawnPoint sp;

        std::string groups_str = file_content.substr(groups_pos, groups_end - groups_pos);
        bool is_spawnpoint = false;
        
        // Check for spawn type
        for (const auto& [spawn_type_str, spawn_type_enum] : spawn_groups) {
            if (groups_str.find(spawn_type_str) != std::string::npos) {
                is_spawnpoint = true;
                sp.spawn_type = spawn_type_enum;
                break;
            }
        }

        // Check for team ID (legacy: team_id appears as a group string)
        // TODO: Verify team ID parsing logic - currently only checks spawn_groups for team assignment
        for (const auto& [group_name, team_enum] : spawn_groups) {
            if (groups_str.find(group_name) != std::string::npos) {
                sp.team_id = team_enum;
                break;
            }
        }

        if (!is_spawnpoint) {
            pos = spawn_pos + 1;
            continue; // Not a spawnpoint
        }
        
        
        // Parse position
        std::string position_string = "position = Vector3(";
        size_t position_pos = file_content.find(position_string, spawn_pos);
        if (position_pos != std::string::npos) {
            position_pos += position_string.length();
            size_t position_end = file_content.find(")", position_pos);
            if (position_end != std::string::npos) {
                std::string position_data = file_content.substr(position_pos, position_end - position_pos);
                std::vector<float> coords;
                size_t coord_pos = 0;
                while (coord_pos < position_data.length()) {
                    // Skip whitespace and commas
                    while (coord_pos < position_data.length() && (std::isspace(position_data[coord_pos]) || position_data[coord_pos] == ',')) {
                        coord_pos++;
                    }
                    
                    if (coord_pos >= position_data.length()) break;
                    
                    // Find end of number
                    size_t start = coord_pos;
                    while (coord_pos < position_data.length() && (std::isdigit(position_data[coord_pos]) || position_data[coord_pos] == '-' || position_data[coord_pos] == '.')) {
                        coord_pos++;
                    }
                    
                    std::string num_str = position_data.substr(start, coord_pos - start);
                    if (!num_str.empty()) {
                        try {
                            coords.push_back(std::stof(num_str));
                        } catch (...) {
                            LOG_WARN("Failed to parse spawnpoint coordinate: %s", num_str.c_str());
                        }
                    }
                }
                
                if (coords.size() >= 3) {
                    sp.position = Vec2(coords[0], coords[2]);
                }
            }
        }
        
        spawnpoints.push_back(sp);
        pos = spawn_pos + 1;
    }
    
    return spawnpoints;
}

std::vector<Vec2> MapSystem::parse_vertices_to_2D(const std::string& vertices_data_raw) {
    std::vector<Vec2> vertices;
    std::vector<float> coords;
    size_t pos = 0;
    // Add all vertices to coords
    while (pos < vertices_data_raw.length()) {
        // Skip whitespace
        while (pos < vertices_data_raw.length() && std::isspace(vertices_data_raw[pos])) {
            pos++;
        }
        
        if (pos >= vertices_data_raw.length()) break;
        
        // Negative numbers
        size_t start = pos;
        if (vertices_data_raw[pos] == '-') pos++;
        
        // Find end of number
        while (pos < vertices_data_raw.length() && (std::isdigit(vertices_data_raw[pos]) || vertices_data_raw[pos] == '.')) {
            pos++;
        }
        
        // Add number to coords
        std::string num_str = vertices_data_raw.substr(start, pos - start);
        if (!num_str.empty()) {
            try {
                coords.push_back(std::stof(num_str));
            } catch (...) {
                LOG_WARN("Failed to parse vertex coordinate: %s", num_str.c_str());
            }
        }
        
        // Skip comma if present
        if (pos < vertices_data_raw.length() && vertices_data_raw[pos] == ',') {
            pos++;
        }
    }
    
    // Parse Vec2 from Vec3 data (expects X, Y, Z coordinates, we use X and Z)
    for (size_t i = 0; i + 2 < coords.size(); i += 3) {
        vertices.push_back(Vec2(coords[i], coords[i + 2]));
    }

    return vertices;
}

std::vector<std::vector<uint32_t>> MapSystem::parse_polygons(const std::string& polygons_data_raw) {
    std::vector<std::vector<uint32_t>> polygons;
    
    // Parse PackedInt32Array(...) entries
    constexpr size_t PACKED_ARRAY_PREFIX_LEN = 17; // Length of "PackedInt32Array("
    size_t pos = 0;
    while (pos < polygons_data_raw.length()) {
        // Find start of PackedInt32Array
        size_t array_start = polygons_data_raw.find("PackedInt32Array(", pos);
        if (array_start == std::string::npos) break;
        
        array_start += PACKED_ARRAY_PREFIX_LEN;
        size_t array_end = polygons_data_raw.find(")", array_start);
        if (array_end == std::string::npos) break;

        std::string array_str = polygons_data_raw.substr(array_start, array_end - array_start);
        std::vector<uint32_t> polygon;
        
        // Parse indices
        size_t idx_pos = 0;
        while (idx_pos < array_str.length()) {
            // Skip whitespace and commas
            while (idx_pos < array_str.length() && (std::isspace(array_str[idx_pos]) || array_str[idx_pos] == ',')) {
                idx_pos++;
            }
            
            if (idx_pos >= array_str.length()) break;
            
            // Find end of number
            size_t start = idx_pos;
            while (idx_pos < array_str.length() && std::isdigit(array_str[idx_pos])) {
                idx_pos++;
            }
            
            std::string num_str = array_str.substr(start, idx_pos - start);
            if (!num_str.empty()) {
                try {
                    polygon.push_back(std::stoul(num_str));
                } catch (...) {
                    LOG_WARN("Failed to parse polygon index: %s", num_str.c_str());
                }
            }
        }
        
        if (!polygon.empty()) {
            polygons.push_back(polygon);
        }
        
        pos = array_end + 1;
    }
    
    LOG_DEBUG("Parsed %zu polygons", polygons.size());
    return polygons;
}

std::vector<uint8_t> MapSystem::serialize_map(Entity* map_entity) {
    if (!map_entity) {
        LOG_ERROR("Cannot serialize null map entity");
        return {};
    }
    
    Map* map = map_entity->get_component<Map>();
    if (!map) {
        LOG_ERROR("Map entity has no Map component");
        return {};
    }
    
    // Calculate packet size
    // 1 byte (type) + 4 bytes (name length) + name + 4 bytes (vertex count) + 4 bytes (polygon count)
    // Vertex and Polygon data for verification of local asset (client-side)
    uint32_t name_len = (uint32_t)(map->name.length());
    uint32_t vertex_count = (uint32_t)(map->vertices.size());
    uint32_t polygon_count = (uint32_t)(map->polygons.size());
    
    size_t packet_size = 1 + 4 + name_len + 4 + 4;
    
    std::vector<uint8_t> data(packet_size);
    size_t offset = 0;
    
    // Write packet type
    data[offset++] = (uint8_t)(PACKET_TYPE::MAP_LOAD);
    
    // Write map name length
    std::memcpy(data.data() + offset, &name_len, sizeof(uint32_t));
    offset += 4;
    
    // Write map name
    std::memcpy(data.data() + offset, map->name.c_str(), name_len);
    offset += name_len;
    
    // Write vertex count
    std::memcpy(data.data() + offset, &vertex_count, sizeof(uint32_t));
    offset += 4;

    // Write polygon count
    std::memcpy(data.data() + offset, &polygon_count, sizeof(uint32_t));
    offset += 4;
    
    LOG_INFO("Serialized map '%s' with %zu vertices and %zu polygons into packet (size: %zu bytes)",
             map->name.c_str(), map->vertices.size(), map->polygons.size(), packet_size);
    
    return data;
}

Vec2 MapSystem::calculate_size_from_vertices(const std::vector<Vec2>& vertices) {
    if (vertices.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    
    float min_x = vertices[0].x;
    float max_x = vertices[0].x;
    float min_y = vertices[0].y;
    float max_y = vertices[0].y;
    
    for (const auto& v : vertices) {
        min_x = std::min(min_x, v.x);
        max_x = std::max(max_x, v.x);
        min_y = std::min(min_y, v.y);
        max_y = std::max(max_y, v.y);
    }
    
    return Vec2(max_x - min_x, max_y - min_y);
}