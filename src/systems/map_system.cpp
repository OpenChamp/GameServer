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

Entity* MapSystem::load_map(const std::string& map_name, const std::string& file_path, EntityManager& entity_manager) {
    // Read file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open map file: %s", file_path.c_str());
        return nullptr;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();
    file.close();
    
    // Parse navmesh
    NavMeshData navmesh_data = parse_navmesh_from_tscn(file_content);
    if (navmesh_data.vertices.empty() || navmesh_data.polygons.empty()) {
        LOG_ERROR("Failed to parse navmesh data from map file: %s", file_path.c_str());
        return nullptr;
    }
    
    // Create map entity
    Entity& map_entity = entity_manager.create_entity();
    
    // Add Map component
    auto map = std::make_unique<Map>();
    map->name = map_name;
    map->vertices = navmesh_data.vertices;
    map->polygons = navmesh_data.polygons;
    map_entity.add_component(std::move(map));
    
    // Add NavMesh component for server-side pathfinding
    // Use bounding box of vertices to calculate spawnpoints
    if (!navmesh_data.vertices.empty()) {
        float min_x = navmesh_data.vertices[0].x;
        float max_x = navmesh_data.vertices[0].x;
        float min_z = navmesh_data.vertices[0].z;
        float max_z = navmesh_data.vertices[0].z;
        
        for (const auto& v : navmesh_data.vertices) {
            min_x = (min_x < v.x) ? min_x : v.x;
            max_x = (max_x > v.x) ? max_x : v.x;
            min_z = (min_z < v.z) ? min_z : v.z;
            max_z = (max_z > v.z) ? max_z : v.z;
        }
        
        auto navmesh = std::make_unique<NavMesh>();
        navmesh->width = max_x - min_x;
        navmesh->height = max_z - min_z;
        navmesh->y_level = 0.0f;
        
        // Set spawnpoints at opposite ends
        navmesh->spawnpoints.push_back(Vec3(min_x, 0.0f, (min_z + max_z) / 2.0f));
        navmesh->spawnpoints.push_back(Vec3(max_x, 0.0f, (min_z + max_z) / 2.0f));
        
        map_entity.add_component(std::move(navmesh));
    }
    
    LOG_INFO("Loaded map '%s' with %zu vertices and %zu polygons",
             map_name.c_str(), navmesh_data.vertices.size(), navmesh_data.polygons.size());
    
    return &map_entity;
}

Entity* MapSystem::load_debug_map(EntityManager& entity_manager) {
    // Create the default Konda map procedurally
    Entity& map_entity = entity_manager.create_entity();
    
    auto map = std::make_unique<Map>();
    map->name = "DEBUG_MAP";
    
    // Create a simple 100x20 navmesh
    // Vertices for a rectangular navmesh
    map->vertices.push_back(Vec3(0.0f, 0.0f, 0.0f));
    map->vertices.push_back(Vec3(100.0f, 0.0f, 0.0f));
    map->vertices.push_back(Vec3(100.0f, 0.0f, 20.0f));
    map->vertices.push_back(Vec3(0.0f, 0.0f, 20.0f));
    
    // Polygons (triangle indices)
    map->polygons.push_back({0, 1, 2});
    map->polygons.push_back({0, 2, 3});
    
    map_entity.add_component(std::move(map));
    
    // Add NavMesh component for server-side pathfinding
    auto navmesh = std::make_unique<NavMesh>();
    navmesh->width = 100.0f;
    navmesh->height = 20.0f;
    navmesh->y_level = 0.0f;
    navmesh->spawnpoints.push_back(Vec3(0.0f, 0.0f, 10.0f));
    navmesh->spawnpoints.push_back(Vec3(100.0f, 0.0f, 10.0f));
    
    map_entity.add_component(std::move(navmesh));
    
    LOG_INFO("Loaded debug map (100x20)");
    
    return &map_entity;
}

Entity* MapSystem::load_default_map(EntityManager& entity_manager) {
    std::vector<std::string> tscn_files;
    std::string map_name;
    std::string file_path;
    // Get all tscn files in the current directory
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        if (entry.is_regular_file() && entry.path().extension() == ".tscn") {
            tscn_files.push_back(entry.path().filename().string());
        }
    }

    if (tscn_files.empty()) {
        LOG_ERROR("No .tscn files found in current directory");
        return load_debug_map(entity_manager);
    }

    file_path = tscn_files[0];
    map_name = std::filesystem::path(file_path).stem().string();
    // Read file
    std::ifstream file(file_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open map file: %s", file_path.c_str());
        return nullptr;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string file_content = buffer.str();
    file.close();
    
    // Parse navmesh
    NavMeshData navmesh_data = parse_navmesh_from_tscn(file_content);
    if (navmesh_data.vertices.empty() || navmesh_data.polygons.empty()) {
        LOG_ERROR("Failed to parse navmesh data from map file: %s", file_path.c_str());
        return nullptr;
    }
    
    // Create map entity
    Entity& map_entity = entity_manager.create_entity();
    
    // Add Map component
    auto map = std::make_unique<Map>();
    map->name = map_name;
    map->vertices = navmesh_data.vertices;
    map->polygons = navmesh_data.polygons;
    map_entity.add_component(std::move(map));
    
    // Add NavMesh component for server-side pathfinding
    // Use bounding box of vertices to calculate spawnpoints
    if (!navmesh_data.vertices.empty()) {
        float min_x = navmesh_data.vertices[0].x;
        float max_x = navmesh_data.vertices[0].x;
        float min_z = navmesh_data.vertices[0].z;
        float max_z = navmesh_data.vertices[0].z;
        
        for (const auto& v : navmesh_data.vertices) {
            min_x = (min_x < v.x) ? min_x : v.x;
            max_x = (max_x > v.x) ? max_x : v.x;
            min_z = (min_z < v.z) ? min_z : v.z;
            max_z = (max_z > v.z) ? max_z : v.z;
        }
        
        auto navmesh = std::make_unique<NavMesh>();
        navmesh->width = max_x - min_x;
        navmesh->height = max_z - min_z;
        navmesh->y_level = 0.0f;
        
        // Set spawnpoints at opposite ends
        navmesh->spawnpoints.push_back(Vec3(min_x, 0.0f, (min_z + max_z) / 2.0f));
        navmesh->spawnpoints.push_back(Vec3(max_x, 0.0f, (min_z + max_z) / 2.0f));
        
        map_entity.add_component(std::move(navmesh));
    }
    
    LOG_INFO("Loaded map '%s' with %zu vertices and %zu polygons",
             map_name.c_str(), navmesh_data.vertices.size(), navmesh_data.polygons.size());
    
    return &map_entity;
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
    size_t vertices_pos = file_content.find("vertices = PackedVector3Array(", nav_mesh_pos);
    if (vertices_pos == std::string::npos) {
        LOG_WARN("vertices line not found in NavigationMesh");
        return data;
    }
    
    vertices_pos += 30; // Skip "vertices = PackedVector3Array("
    size_t vertices_end = file_content.find(")", vertices_pos);
    if (vertices_end == std::string::npos) {
        LOG_WARN("vertices end not found");
        return data;
    }
    
    std::string vertices_str = file_content.substr(vertices_pos, vertices_end - vertices_pos);
    data.vertices = parse_vertices(vertices_str);
    
    // Find polygons line
    size_t polygons_pos = file_content.find("polygons = [", nav_mesh_pos);
    if (polygons_pos == std::string::npos) {
        LOG_WARN("polygons line not found in NavigationMesh");
        return data;
    }
    
    polygons_pos += 12; // Skip "polygons = ["
    size_t polygons_end = file_content.find("]", polygons_pos);
    if (polygons_end == std::string::npos) {
        LOG_WARN("polygons end not found");
        return data;
    }
    
    std::string polygons_str = file_content.substr(polygons_pos, polygons_end - polygons_pos);
    data.polygons = parse_polygons(polygons_str);
    
    return data;
}

std::vector<Vec3> MapSystem::parse_vertices(const std::string& vertices_str) {
    std::vector<Vec3> vertices;
    std::vector<float> coords;
    
    // Parse numbers separated by commas and spaces
    size_t pos = 0;
    while (pos < vertices_str.length()) {
        // Skip whitespace
        while (pos < vertices_str.length() && std::isspace(vertices_str[pos])) {
            pos++;
        }
        
        if (pos >= vertices_str.length()) break;
        
        // Handle negative numbers
        size_t start = pos;
        if (vertices_str[pos] == '-') pos++;
        
        // Find end of number
        while (pos < vertices_str.length() && (std::isdigit(vertices_str[pos]) || vertices_str[pos] == '.')) {
            pos++;
        }
        
        std::string num_str = vertices_str.substr(start, pos - start);
        if (!num_str.empty()) {
            try {
                coords.push_back(std::stof(num_str));
            } catch (...) {
                LOG_WARN("Failed to parse vertex coordinate: %s", num_str.c_str());
            }
        }
        
        // Skip comma if present
        if (pos < vertices_str.length() && vertices_str[pos] == ',') {
            pos++;
        }
    }
    
    // Group coordinates into Vec3 (x, y, z)
    for (size_t i = 0; i + 2 < coords.size(); i += 3) {
        vertices.push_back(Vec3(coords[i], coords[i + 1], coords[i + 2]));
    }
    
    LOG_DEBUG("Parsed %zu vertices", vertices.size());
    return vertices;
}

std::vector<std::vector<uint32_t>> MapSystem::parse_polygons(const std::string& polygons_str) {
    std::vector<std::vector<uint32_t>> polygons;
    
    // Parse PackedInt32Array(...) entries
    size_t pos = 0;
    while (pos < polygons_str.length()) {
        // Find start of PackedInt32Array
        size_t array_start = polygons_str.find("PackedInt32Array(", pos);
        if (array_start == std::string::npos) break;
        
        array_start += 17; // Skip "PackedInt32Array("
        size_t array_end = polygons_str.find(")", array_start);
        if (array_end == std::string::npos) break;
        
        std::string array_str = polygons_str.substr(array_start, array_end - array_start);
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

