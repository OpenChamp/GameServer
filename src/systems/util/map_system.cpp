#include <systems/util/map_system.hpp>
#include <components/map.hpp>
#include <components/navmesh.hpp>
#include <libs/log.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <vector>
#include <array>
#include <tuple>
#include <cmath>


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

    // Parse spawnpoints
    std::vector<SpawnPoint> spawnpoints = parse_spawnpoints_from_tscn(file_content);
    LOG_INFO("Parsed %zu spawnpoints from map file", spawnpoints.size());
    
    // Extract spawnpoint positions for transformation
    std::vector<Vec2> spawnpoint_positions;
    for (const auto& sp : spawnpoints) {
        spawnpoint_positions.push_back(sp.position);
    }
    
    // Apply NavigationRegion3D transform to navmesh and spawnpoints
    apply_navigation_region_transform(file_content, navmesh_data.vertices, spawnpoint_positions);
    
    // Update spawnpoints with transformed positions
    for (size_t i = 0; i < spawnpoints.size() && i < spawnpoint_positions.size(); ++i) {
        spawnpoints[i].position = spawnpoint_positions[i];
    }
    
    // Add Map component
    Map map;
    size_t final_slash_index = actual_file_path.find_last_of("/\\");
    // Get name from file
    map.name = actual_file_path.substr(final_slash_index + 1, actual_file_path.find_last_of('.') - final_slash_index - 1);
    map.vertices = navmesh_data.vertices;
    map.polygons = navmesh_data.polygons;
    // Store spawnpoints directly (already Vec2 from parsing)
    map.spawnpoints.reserve(spawnpoints.size());
    for (const auto& sp : spawnpoints) {
        map.spawnpoints.push_back(sp.position);
        LOG_INFO("  Spawnpoint: (%.1f, %.1f)", sp.position.x, sp.position.y);
    }
    
    // Calculate map bounds and derived size/offset
    MapBounds bounds = MapSystem::calculate_bounds_from_vertices(navmesh_data.vertices);
    map.size = Vec2(bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y);
    // Offset is the center point between min and max
    map.offset = Vec2((bounds.min.x + bounds.max.x) / 2.0f, (bounds.min.y + bounds.max.y) / 2.0f);

    // Build spatial acceleration grid for fast polygon lookups
    MapSystem::build_polygon_grid(map, 1.0f);

    LOG_INFO("Loaded map '%s' with %zu vertices, %zu polygons, and %zu spawnpoints",
             map.name.c_str(), navmesh_data.vertices.size(), navmesh_data.polygons.size(), spawnpoints.size());

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
        size_t groups_pos = file_content.find("groups=[", spawn_pos);
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
                sp.type = spawn_type_enum;
                LOG_INFO("Marker3D node at pos %zu is a spawnpoint of type %d", spawn_pos, spawn_type_enum);
                break;
            }
        }

        // Check for team ID (legacy: team_id appears as a group string)
        // TODO: Verify team ID parsing logic - currently only checks spawn_groups for team assignment
        for (const auto& [group_name, team_id] : spawn_teams) {
            if (groups_str.find(group_name) != std::string::npos) {
                sp.team_id = team_id;
                break;
            }
        }

        if (!is_spawnpoint) {
            pos = spawn_pos + 1;
            continue; // Not a spawnpoint
        }
        
        
        // Parse position
        std::string position_string = "Transform3D(";
        size_t position_pos = file_content.find(position_string, spawn_pos);
        if (position_pos == std::string::npos) {
            LOG_WARN("SpawnPoint missing position data");
            pos = spawn_pos + 1;
            continue;
        }
        position_pos += position_string.length(); // Skip "Transform3D("
        size_t position_end = file_content.find(")", position_pos);
        if (position_end == std::string::npos) {
            LOG_WARN("SpawnPoint missing position data");
            pos = spawn_pos + 1;
            continue;
        }

        std::string position_data = file_content.substr(position_pos, position_end - position_pos);
        // Parse all values from Transform3D(m00, m01, ..., m33)
        // Godot Transform3D has 12 values: 3x3 matrix (9) + position (3)
        // Position is in the last 3 values (indices 9, 10, 11 which are x, y, z)
        std::vector<float> matrix_vals;
        size_t tpos = 0;

        // Parse all floats
        while (tpos < position_data.length()) {
            // Skip whitespace
            while (tpos < position_data.length() && std::isspace(position_data[tpos])) {
                tpos++;
            }
            
            if (tpos >= position_data.length()) break;
            
            // Negative numbers
            size_t start = tpos;
            if (position_data[tpos] == '-') tpos++;
            
            // Find end of number
            while (tpos < position_data.length() && (std::isdigit(position_data[tpos]) || position_data[tpos] == '.')) {
                tpos++;
            }
            
            // Add number to coords
            std::string num_str = position_data.substr(start, tpos - start);
            if (!num_str.empty()) {
                try {
                    matrix_vals.push_back(std::stof(num_str));
                } catch (...) {
                    LOG_WARN("Failed to parse spawnpoint matrix value: %s", num_str.c_str());
                }
            }
            
            // Skip comma if present
            if (tpos < position_data.length() && position_data[tpos] == ',') {
                tpos++;
            }
        }

        // Set spawnpoint position from Transform3D matrix
        // Transform3D has 12 values: indices 9, 10, 11 are x, y, z translation
        if (matrix_vals.size() >= 12) {
            sp.position = Vec2(matrix_vals[9], matrix_vals[11]);  // x and z coordinates
            LOG_INFO("Parsed spawnpoint position: (%.1f, %.1f)", sp.position.x, sp.position.y);
        } else {
            LOG_WARN("SpawnPoint position data malformed - got %zu values instead of 12", matrix_vals.size());
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

MapSystem::MapBounds MapSystem::calculate_bounds_from_vertices(const std::vector<Vec2>& vertices) {
    MapBounds bounds;
    
    if (vertices.empty()) {
        bounds.min = Vec2(0.0f, 0.0f);
        bounds.max = Vec2(0.0f, 0.0f);
        return bounds;
    }
    
    bounds.min.x = vertices[0].x;
    bounds.max.x = vertices[0].x;
    bounds.min.y = vertices[0].y;
    bounds.max.y = vertices[0].y;
    
    for (const auto& v : vertices) {
        bounds.min.x = std::min(bounds.min.x, v.x);
        bounds.max.x = std::max(bounds.max.x, v.x);
        bounds.min.y = std::min(bounds.min.y, v.y);
        bounds.max.y = std::max(bounds.max.y, v.y);
    }
    
    return bounds;
}

MapSystem::TransformData 
MapSystem::parse_transform_3d(const std::string& transform_str) {
    // Format: Transform3D(m00, m01, m02, m10, m11, m12, m20, m21, m22, x, y, z)
    MapSystem::TransformData data;
    data.has_rotation = false;
    data.position.x = 0.0f;
    data.position.y = 0.0f;
    
    // Initialize matrix to identity
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            data.matrix[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    
    std::vector<float> values;
    size_t pos = 0;
    
    // Parse all 12 floats from the transform string
    while (pos < transform_str.length() && values.size() < 12) {
        // Skip whitespace and commas
        while (pos < transform_str.length() && (std::isspace(transform_str[pos]) || transform_str[pos] == ',')) {
            pos++;
        }
        
        if (pos >= transform_str.length()) break;
        
        // Parse number
        size_t start = pos;
        if (transform_str[pos] == '-') pos++;
        
        // Parse digits and decimal point
        while (pos < transform_str.length() && (std::isdigit(transform_str[pos]) || transform_str[pos] == '.')) {
            pos++;
        }
        
        // Handle scientific notation (e or E)
        if (pos < transform_str.length() && (transform_str[pos] == 'e' || transform_str[pos] == 'E')) {
            pos++;
            // Handle optional sign after e
            if (pos < transform_str.length() && (transform_str[pos] == '+' || transform_str[pos] == '-')) {
                pos++;
            }
            // Parse exponent digits
            while (pos < transform_str.length() && std::isdigit(transform_str[pos])) {
                pos++;
            }
        }
        
        if (pos > start) {
            try {
                values.push_back(std::stof(transform_str.substr(start, pos - start)));
            } catch (...) {
                LOG_WARN("Failed to parse transform value from '%s'", transform_str.substr(start, pos - start).c_str());
            }
        }
    }
    
    if (values.size() >= 12) {
        // Extract 3x3 rotation matrix
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                data.matrix[i][j] = values[i * 3 + j];
            }
        }
        
        // Extract position (last 3 values)
        data.position.x = values[9];
        data.position.y = values[11];  // z coordinate becomes y
        
        // Check if there's any significant rotation (check if matrix is not identity)
        const float eps = 0.0001f;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                float expected = (i == j) ? 1.0f : 0.0f;
                if (std::abs(data.matrix[i][j] - expected) > eps) {
                    data.has_rotation = true;
                    break;
                }
            }
            if (data.has_rotation) break;
        }
        
        if (data.has_rotation) {
            LOG_INFO("Detected rotation in NavigationRegion3D: matrix=[[%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f]]",
                     data.matrix[0][0], data.matrix[0][1], data.matrix[0][2],
                     data.matrix[1][0], data.matrix[1][1], data.matrix[1][2],
                     data.matrix[2][0], data.matrix[2][1], data.matrix[2][2]);
        }
    } else {
        LOG_WARN("Failed to parse Transform3D - expected 12 values, got %zu", values.size());
    }
    
    return data;
}

void MapSystem::apply_navigation_region_transform(const std::string& file_content, 
                                                   std::vector<Vec2>& vertices,
                                                   std::vector<Vec2>& spawnpoints) {
    // Find the NavigationRegion3D node and extract its transform
    size_t nav_region_pos = file_content.find("[node name=\"");
    if (nav_region_pos == std::string::npos) {
        LOG_WARN("No node found in tscn file");
        return;
    }
    
    // Find the first node's transform line (should be the root NavigationRegion3D)
    size_t transform_pos = file_content.find("transform = Transform3D(", nav_region_pos);
    if (transform_pos == std::string::npos) {
        LOG_DEBUG("No transform found on NavigationRegion3D node (using default identity)");
        return;
    }
    
    // Extract transform string
    transform_pos += std::string("transform = Transform3D(").length();
    size_t transform_end = file_content.find(")", transform_pos);
    if (transform_end == std::string::npos) {
        LOG_WARN("Failed to find end of Transform3D string");
        return;
    }
    
    std::string transform_str = file_content.substr(transform_pos, transform_end - transform_pos);
    MapSystem::TransformData transform_data = parse_transform_3d(transform_str);
    
    if (!transform_data.has_rotation) {
        LOG_DEBUG("NavigationRegion3D has identity rotation (no transformation needed)");
        return;
    }
    
    // Apply rotation to vertices
    // Transform is 3D: (X, Y, Z) -> applying 2D projection
    // Godot uses X-right, Y-up, Z-back coordinate system
    // We extract X and Z (converting Z to Y in 2D)
    for (auto& v : vertices) {
        float x3d = v.x;
        float z3d = v.y;  // Y in 2D is Z in 3D
        float y3d = 0.0f; // Navmesh is flat on Y=0 plane
        
        // Apply rotation: result = matrix * [x, y, z]
        float new_x = transform_data.matrix[0][0] * x3d + transform_data.matrix[0][1] * y3d + transform_data.matrix[0][2] * z3d;
        float new_z = transform_data.matrix[2][0] * x3d + transform_data.matrix[2][1] * y3d + transform_data.matrix[2][2] * z3d;
        
        // Store back as 2D (X and Z become X and Y)
        v.x = new_x;
        v.y = new_z;
    }
    
    // Apply rotation to spawnpoints
    for (auto& sp : spawnpoints) {
        float x3d = sp.x;
        float z3d = sp.y;
        float y3d = 0.0f; // Spawnpoints are on the ground plane
        
        float new_x = transform_data.matrix[0][0] * x3d + transform_data.matrix[0][1] * y3d + transform_data.matrix[0][2] * z3d;
        float new_z = transform_data.matrix[2][0] * x3d + transform_data.matrix[2][1] * y3d + transform_data.matrix[2][2] * z3d;
        
        sp.x = new_x;
        sp.y = new_z;
    }
    
    LOG_INFO("Applied NavigationRegion3D transform to %zu vertices and %zu spawnpoints", 
             vertices.size(), spawnpoints.size());
}

void MapSystem::build_polygon_grid(Map& map, float cell_size) {
    if (map.vertices.empty() || map.polygons.empty() || cell_size <= 0.0f) {
        LOG_WARN("build_polygon_grid: Invalid input (vertices: %zu, polygons: %zu, cell_size: %.1f)",
                 map.vertices.size(), map.polygons.size(), cell_size);
        return;
    }

    map.grid_cell_size = cell_size;

    // Find bounding box of all vertices
    Vec2 min_bounds = map.vertices[0];
    Vec2 max_bounds = map.vertices[0];

    for (const auto& vertex : map.vertices) {
        min_bounds.x = std::min(min_bounds.x, vertex.x);
        min_bounds.y = std::min(min_bounds.y, vertex.y);
        max_bounds.x = std::max(max_bounds.x, vertex.x);
        max_bounds.y = std::max(max_bounds.y, vertex.y);
    }

    // Add padding to bounds to handle edge cases
    const float PADDING = cell_size * 0.5f;
    min_bounds.x -= PADDING;
    min_bounds.y -= PADDING;
    max_bounds.x += PADDING;
    max_bounds.y += PADDING;

    map.grid_origin = min_bounds;

    // Calculate grid dimensions
    Vec2 bounds_size = max_bounds - min_bounds;
    map.grid_width = std::max(1, static_cast<int>(std::ceil(bounds_size.x / cell_size)));
    map.grid_height = std::max(1, static_cast<int>(std::ceil(bounds_size.y / cell_size)));

    // Initialize grid: grid_cells[y * grid_width + x] = {polygon_ids}
    map.grid_cells.resize(map.grid_width * map.grid_height);

    LOG_DEBUG("build_polygon_grid: Grid dimensions %dx%d (cell_size: %.1f, bounds: (%.1f,%.1f) to (%.1f,%.1f))",
             map.grid_width, map.grid_height, cell_size,
             min_bounds.x, min_bounds.y, max_bounds.x, max_bounds.y);

    // Helper lambda to get polygon bounds
    auto GetPolygonBounds = [&](const std::vector<uint32_t>& polygon, Vec2& out_min, Vec2& out_max) {
        if (polygon.empty()) {
            out_min = Vec2(0.0f, 0.0f);
            out_max = Vec2(0.0f, 0.0f);
            return;
        }

        uint32_t first_idx = polygon[0];
        if (first_idx >= map.vertices.size()) {
            out_min = Vec2(0.0f, 0.0f);
            out_max = Vec2(0.0f, 0.0f);
            return;
        }

        out_min = map.vertices[first_idx];
        out_max = map.vertices[first_idx];

        for (uint32_t vertex_idx : polygon) {
            if (vertex_idx >= map.vertices.size()) {
                continue;
            }

            const Vec2& vertex = map.vertices[vertex_idx];
            out_min.x = std::min(out_min.x, vertex.x);
            out_min.y = std::min(out_min.y, vertex.y);
            out_max.x = std::max(out_max.x, vertex.x);
            out_max.y = std::max(out_max.y, vertex.y);
        }
    };

    // Helper lambda to convert world position to grid coordinates
    auto WorldToGridCoords = [&](const Vec2& world_pos, int& out_x, int& out_y) {
        Vec2 relative_pos = world_pos - map.grid_origin;
        
        out_x = static_cast<int>(std::floor(relative_pos.x / cell_size));
        out_y = static_cast<int>(std::floor(relative_pos.y / cell_size));

        // Clamp to valid grid range
        out_x = std::max(0, std::min(out_x, map.grid_width - 1));
        out_y = std::max(0, std::min(out_y, map.grid_height - 1));
    };

    // Add each polygon to grid cells it overlaps
    for (uint32_t poly_id = 0; poly_id < map.polygons.size(); ++poly_id) {
        Vec2 poly_min, poly_max;
        GetPolygonBounds(map.polygons[poly_id], poly_min, poly_max);

        // Convert polygon bounds to grid coordinates
        int min_x, min_y, max_x, max_y;
        WorldToGridCoords(poly_min, min_x, min_y);
        WorldToGridCoords(poly_max, max_x, max_y);

        // Add polygon ID to all grid cells it overlaps
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                int index = y * map.grid_width + x;
                map.grid_cells[index].push_back(poly_id);
            }
        }
    }

    // Log statistics
    int total_entries = 0;
    int non_empty_cells = 0;
    for (const auto& cell : map.grid_cells) {
        if (!cell.empty()) {
            non_empty_cells++;
            total_entries += cell.size();
        }
    }

    LOG_INFO("Polygon grid built: %d non-empty cells, %.1f polygons per cell average",
             non_empty_cells, non_empty_cells > 0 ? static_cast<float>(total_entries) / non_empty_cells : 0.0f);
}