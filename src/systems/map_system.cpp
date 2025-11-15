#include "map_system.hpp"
#include "components/map.hpp"
#include "components/navmesh.hpp"
#include <log.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <vector>

EntityID MapSystem::load_map(const std::string& map_name, const std::string& file_path, EntityManager& entity_manager) {
    std::string actual_file_path = file_path;
    std::string actual_map_name = map_name;
    
    // If no path provided or file doesn't exist, scan for .tscn files in current directory
    if (actual_file_path.empty() || !std::filesystem::exists(actual_file_path)) {
        if (!actual_file_path.empty()) {
            LOG_WARN("Map file not found: %s. Scanning current directory for .tscn files...", actual_file_path.c_str());
        }
        
        std::vector<std::string> tscn_files;
        for (const auto& entry : std::filesystem::directory_iterator(".")) {
            if (entry.is_regular_file() && entry.path().extension() == ".tscn") {
                tscn_files.push_back(entry.path().filename().string());
            }
        }
        
        if (tscn_files.empty()) {
            LOG_ERROR("No .tscn files found in current directory. Cannot load map.");
            return nullptr;
        }
        
        actual_file_path = tscn_files[0];
        actual_map_name = std::filesystem::path(actual_file_path).stem().string();
        LOG_INFO("Found .tscn file: %s", actual_file_path.c_str());
    }
    
    // Read file
    std::ifstream file(actual_file_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open map file: %s", actual_file_path.c_str());
        return NULL;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();
    file.close();
    
    // Parse navmesh
    NavMeshData navmesh_data = parse_navmesh_from_tscn(file_content);
    if (navmesh_data.vertices.empty() || navmesh_data.polygons.empty()) {
        LOG_ERROR("Failed to parse navmesh data from map file: %s", actual_file_path.c_str());
        return NULL;
    }
    
    // Create map entity
    Entity& map_entity = entity_manager.create_entity();
    // FIX: Cleaning up from dirty pool, but should probably handle this differently -- cmkrist 15/11/2025
    entity_manager.mark_entity_clean(map_entity.get_id());
    
    // Add Map component
    auto map = std::make_unique<Map>();
    map->name = actual_map_name;
    map->vertices = navmesh_data.vertices;
    map->polygons = navmesh_data.polygons;
    // Get 2d size and offset
    map->size = calculate_size_from_vertices(navmesh_data.vertices);
    map->offset = map->size / 2.0f;

    map_entity.add_component(std::move(map));

    LOG_INFO("Loaded map '%s' with %zu vertices and %zu polygons",
             actual_map_name.c_str(), navmesh_data.vertices.size(), navmesh_data.polygons.size());
    
    return map_entity.get_id();
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

std::vector<Vec2> MapSystem::parse_vertices_to_2D(const std::string& vertices_data_raw) {
    std::vector<Vec2> vertices;
    std::vector<float> coords;
    std::vector<Vec2> map_size = {Vec2(0.0f, 0.0f), Vec2(0.0f, 0.0f)};
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
    
    // Parse Vec2 from Vec3 data
    for (size_t i = 0; i + 2 < coords.size(); i += 3) {
        vertices.push_back(Vec2(coords[i], coords[i + 2]));
    }

    return vertices;
}

std::vector<std::vector<uint32_t>> MapSystem::parse_polygons(const std::string& polygons_data_raw) {
    std::vector<std::vector<uint32_t>> polygons;
    
    // Parse PackedInt32Array(...) entries
    size_t pos = 0;
    while (pos < polygons_data_raw.length()) {
        // Find start of PackedInt32Array
        size_t array_start = polygons_data_raw.find("PackedInt32Array(", pos);
        if (array_start == std::string::npos) break;
        
        array_start += 17; // Skip "PackedInt32Array("
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

ENetPacket* MapSystem::serialize_map(Entity* map_entity) {
    if (!map_entity) {
        LOG_ERROR("Cannot serialize null map entity");
        return nullptr;
    }
    
    Map* map = map_entity->get_component<Map>();
    if (!map) {
        LOG_ERROR("Map entity has no Map component");
        return nullptr;
    }
    
    // Calculate packet size
    // 1 byte (type) + 4 bytes (name length) + name + 4 bytes (vertex count) + 4 bytes (polygon count)
    // Vertex and Polygon data for verification of local asset (client-side)
    uint32_t name_len = (uint32_t)(map->name.length());
    uint32_t vertex_count = (uint32_t)(map->vertices.size());
    uint32_t polygon_count = (uint32_t)(map->polygons.size());
    
    size_t packet_size = 1 + 4 + name_len + 4 + 4;
    
    // Create packet
    ENetPacket* packet = enet_packet_create(nullptr, packet_size, ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        LOG_ERROR("Failed to allocate map state packet");
        return nullptr;
    }
    
    uint8_t* data = (uint8_t*)(packet->data);
    size_t offset = 0;
    
    // Write packet type
    data[offset++] = (uint8_t)(PACKET_TYPE::SPAWN_MAP);
    
    // Write map name length
    std::memcpy(data + offset, &name_len, sizeof(uint32_t));
    offset += 4;
    
    // Write map name
    std::memcpy(data + offset, map->name.c_str(), name_len);
    offset += name_len;
    
    // Write vertex count
    std::memcpy(data + offset, &vertex_count, sizeof(uint32_t));
    offset += 4;

    // Write polygon count
    std::memcpy(data + offset, &polygon_count, sizeof(uint32_t));
    offset += 4;
    
    LOG_INFO("Serialized map '%s' with %zu vertices and %zu polygons into packet (size: %zu bytes)",
             map->name.c_str(), map->vertices.size(), map->polygons.size(), packet_size);
    
    return packet;
}

Vec2 calculate_size_from_vertices(const std::vector<Vec2>& vertices) {
    if (vertices.empty()) {
        return Vec2(0.0f, 0.0f);
    }
    
    float min_x = vertices[0].x;
    float max_x = vertices[0].x;
    float min_y = vertices[0].y;
    float max_y = vertices[0].y;
    
    for (const auto& v : vertices) {
        min_x = (min_x < v.x) ? min_x : v.x;
        max_x = (max_x > v.x) ? max_x : v.x;
        min_y = (min_y < v.y) ? min_y : v.y;
        max_y = (max_y > v.y) ? max_y : v.y;
    }
    
    return Vec2(max_x - min_x, max_y - min_y);
}