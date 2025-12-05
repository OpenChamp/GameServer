#include <systems/util/data_loader.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <cmath>

#include <libs/pugixml.hpp>
#include <components/component_registry.hpp>
#include <components/stats.hpp>
#include <components/network_entity.hpp>
#include <components/entity_state.hpp>
#include <components/npc_component.hpp>
#include <components/target.hpp>
#include <components/attack.hpp>
#include <components/intent.hpp>
#include <components/metadata.hpp>

/**
 * Convert string to DamageType enum.
 * @param str The string to convert (case-insensitive)
 * @return DamageType enum value, defaults to PHYSICAL if unrecognized
 */
static DamageType string_to_damage_type(const std::string& str) {
    if (str == "magical" || str == "magic") {
        return DamageType::MAGICAL;
    } else if (str == "true_damage" || str == "true") {
        return DamageType::TRUE_DAMAGE;
    }
    // Default to PHYSICAL for unrecognized or empty strings
    return DamageType::PHYSICAL;
}

/**
 * Convert string to NPCType enum.
 * @param str The string to convert (case-insensitive)
 * @return NPCType enum value, defaults to MINION if unrecognized
 */
static NPCType string_to_npc_type(const std::string& str) {
    if (str == "minion") {
        return NPCType::MINION;
    }
    // Default to MINION for unrecognized or empty strings
    return NPCType::MINION;
}

/**
 * Convert string to IntentType enum.
 * @param str The string to convert (case-insensitive)
 * @return IntentType enum value, defaults to NONE if unrecognized
 */
static IntentType string_to_intent_type(const std::string& str) {
    if (str == "move_to_position") {
        return IntentType::MOVE_TO_POSITION;
    } else if (str == "attack_target") {
        return IntentType::ATTACK_TARGET;
    }
    // Default to NONE for unrecognized or empty strings
    return IntentType::NONE;
}

/**
 * Convenience macro. Loads the attribute from the node if the node has it, and sets it in the component.
 * @param node A pugi::xml_node which (maybe) has the attribute
 * @param comp The Component in which to set the value of the attribute
 * @param attr The attribute to load. Must match exactly both the attribute name in the xml and the property in the Component.
 * @param as pugixml can load attributes as a number of different types, for example string, float or int
 */
#define LOAD_ATTRIBUTE(node, comp, attr, as)                   \
    {                                                          \
        pugi::xml_attribute loaded = node.attribute(#attr);    \
        if(!loaded.empty()) {                                  \
            comp->attr = loaded.as_##as();                     \
        }                                                      \
    }

/**
 * Convenience macro for loading enum attributes from XML.
 * @param node A pugi::xml_node which (maybe) has the attribute
 * @param comp The Component in which to set the value of the attribute
 * @param attr The attribute to load. Must match exactly both the attribute name in the xml and the property in the Component.
 * @param converter A function that converts string to the enum type
 */
#define LOAD_ENUM_ATTRIBUTE(node, comp, attr, converter)       \
    {                                                          \
        pugi::xml_attribute loaded = node.attribute(#attr);    \
        if(!loaded.empty()) {                                  \
            comp->attr = converter(loaded.as_string());        \
        }                                                      \
    }


EntityTemplate DataLoader::load_entity_template(std::string file_name) {
    EntityTemplate temp;

    // NOTE: Component type IDs should match those defined in src/component_registry.hpp
    // See ComponentTypes namespace for authoritative type ID assignments.
    
    // TODO should probably not let pugi load the file for us; thinking .pak files etc... - ploinky 14/11/2025
    pugi::xml_document doc;
    pugi::xml_parse_status status = doc.load_file(file_name.c_str()).status;
    if(status != pugi::xml_parse_status::status_ok) {
        LOG_ERROR("Failed to load entity from data file: %s, pugixml status is %d", file_name.c_str(), status);
        return temp;
    }

    pugi::xml_node rootNode = doc.child("entity");
    if(rootNode == NULL) {
        LOG_ERROR("Failed to load entity from data file: %s, rootNode 'entity' is missing", file_name.c_str());
        return temp;
    }

    std::string id = rootNode.attribute("id").as_string();
    if(id.empty()) {
        LOG_ERROR("Failed to load entity from data file: %s, rootNode 'entity' is missing id attribute", file_name.c_str());
        return temp;
    }
    temp.id = id;

    pugi::xml_node movementNode = rootNode.child("movement");
    if(!movementNode.empty()) {
        std::shared_ptr<Movement> movement = std::make_shared<Movement>();
        temp.component_templates.push_back(movement);
    }
    
    pugi::xml_node pathfindingNode = rootNode.child("pathfinding");
    if(!pathfindingNode.empty()) {
        std::shared_ptr<PathfindingComponent> pathfinding = std::make_shared<PathfindingComponent>();
        temp.component_templates.push_back(pathfinding);
    }

    pugi::xml_node networkNode = rootNode.child("network");
    if(!networkNode.empty()) {
        std::shared_ptr<NetworkEntityComponent> network = std::make_shared<NetworkEntityComponent>();
        network->force_full_sync_next_frame = true;
        temp.component_templates.push_back(network);
    }

    pugi::xml_node stateNode = rootNode.child("state");
    if(!stateNode.empty()) {
        std::shared_ptr<EntityStateComponent> state = std::make_shared<EntityStateComponent>();
        temp.component_templates.push_back(state);
    }

    pugi::xml_node npc_node = rootNode.child("npc");
    if(!npc_node.empty()) {
        std::shared_ptr<NPCComponent> npc = std::make_shared<NPCComponent>();
        LOAD_ENUM_ATTRIBUTE(npc_node, npc, npc_type, string_to_npc_type)
        LOAD_ATTRIBUTE(npc_node, npc, aggression_range, float)
        LOAD_ATTRIBUTE(npc_node, npc, chase_distance, float)
        temp.component_templates.push_back(npc);
    }

    pugi::xml_node meta_node = rootNode.child("meta");
    if(!meta_node.empty()) {
        std::shared_ptr<MetadataComponent> meta = std::make_shared<MetadataComponent>();
        LOAD_ATTRIBUTE(meta_node, meta, name, string)
        LOAD_ATTRIBUTE(meta_node, meta, description, string)
        LOAD_ATTRIBUTE(meta_node, meta, icon, string)
        LOAD_ATTRIBUTE(meta_node, meta, model, string)
        temp.component_templates.push_back(meta);
    }

    pugi::xml_node target_node = rootNode.child("target");
    if(!target_node.empty()) {
        std::shared_ptr<TargetComponent> target = std::make_shared<TargetComponent>();
        LOAD_ATTRIBUTE(target_node, target, target_search_interval_ms, float)
        temp.component_templates.push_back(target);
    }

    pugi::xml_node attack_node = rootNode.child("attack");
    if(!attack_node.empty()) {
        std::shared_ptr<AttackComponent> attack = std::make_shared<AttackComponent>();
        temp.component_templates.push_back(attack);
    }

    pugi::xml_node statsNode = rootNode.child("stats");
    if(!statsNode.empty()) {
        std::shared_ptr<Stats> stats = std::make_shared<Stats>();

        LOAD_ATTRIBUTE(statsNode, stats, max_health, float)
        LOAD_ATTRIBUTE(statsNode, stats, health, float)
        LOAD_ATTRIBUTE(statsNode, stats, max_mana, float)
        LOAD_ATTRIBUTE(statsNode, stats, mana, float)
        LOAD_ATTRIBUTE(statsNode, stats, move_speed, float)
        LOAD_ATTRIBUTE(statsNode, stats, level, int)

        LOAD_ATTRIBUTE(statsNode, stats, attack_range, float)
        LOAD_ATTRIBUTE(statsNode, stats, attack_speed, float)
        // TODO what to do about enums (like DamageType auto_damage_type)? - ploinky 14/11/2025
        //      |
        // This v -- cmkrist 16/11/2025
        LOAD_ENUM_ATTRIBUTE(statsNode, stats, auto_damage_type, string_to_damage_type)
        LOAD_ATTRIBUTE(statsNode, stats, crit_chance, float)
        LOAD_ATTRIBUTE(statsNode, stats, crit_bonus, int)
        LOAD_ATTRIBUTE(statsNode, stats, true_bonus, int)
        LOAD_ATTRIBUTE(statsNode, stats, magic_power, float)
        LOAD_ATTRIBUTE(statsNode, stats, physical_power, float)
        LOAD_ATTRIBUTE(statsNode, stats, projectile_speed, float)

        LOAD_ATTRIBUTE(statsNode, stats, armor, int)
        LOAD_ATTRIBUTE(statsNode, stats, magic_resist, int)
        LOAD_ATTRIBUTE(statsNode, stats, dodge, int)

        LOAD_ATTRIBUTE(statsNode, stats, health_regen, float)
        LOAD_ATTRIBUTE(statsNode, stats, mana_regen, float)
        LOAD_ATTRIBUTE(statsNode, stats, life_steal, float)
        LOAD_ATTRIBUTE(statsNode, stats, spell_vamp, float)
        LOAD_ATTRIBUTE(statsNode, stats, omni_vamp, float)
        LOAD_ATTRIBUTE(statsNode, stats, leech, float)
        LOAD_ATTRIBUTE(statsNode, stats, vision_range, float)

        LOG_DEBUG("Loaded Stats component: health=%f, max_health=%f, move_speed=%f", 
                  stats->health, stats->max_health, stats->move_speed);
        temp.component_templates.push_back(stats);
    }

    pugi::xml_node intent_node = rootNode.child("intent");
    if(!intent_node.empty()) {
        std::shared_ptr<IntentComponent> intent = std::make_shared<IntentComponent>();
        // Parse the initial intent if one is defined
        std::string intent_type_str = intent_node.attribute("type").as_string("");
        if (!intent_type_str.empty()) {
            intent->type = string_to_intent_type(intent_type_str);
            
            // Load optional intent parameters
            LOAD_ENUM_ATTRIBUTE(intent_node, intent, type, string_to_intent_type)
            
            LOG_DEBUG("Loaded Intent component: type=%d", static_cast<int>(intent->type));
        }
        temp.component_templates.push_back(intent);
    }

    LOG_INFO("Entity Template Loaded: \"%s\" || %d components", temp.id.c_str(), temp.component_templates.size());
    return temp;
}

std::pair<std::vector<Vec2>, std::vector<std::vector<uint32_t>>> DataLoader::load_navmesh_obj(const std::string& nav_file_path) {
    std::vector<Vec2> vertices;
    std::vector<std::vector<uint32_t>> polygons;
    
    std::ifstream file(nav_file_path);
    if (!file.is_open()) {
        LOG_ERROR("Failed to open navmesh OBJ file: %s", nav_file_path.c_str());
        return { vertices, polygons };
    }
    
    std::string line;
    uint32_t line_number = 0;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;
        
        // Parse vertex (v x y z)
        if (prefix == "v") {
            float x, y, z;
            if (!(iss >> x >> y >> z)) {
                LOG_ERROR("Failed to parse vertex at line %u in: %s", line_number, nav_file_path.c_str());
                continue;
            }
            // Store as Vec2, using X and Z coordinates (Y is elevation)
            vertices.push_back(Vec2(x, z));
        }
        // Parse face (f v1 v2 v3 ...)
        else if (prefix == "f") {
            std::vector<uint32_t> polygon;
            std::string vertex_str;
            
            while (iss >> vertex_str) {
                // OBJ indices are 1-based, convert to 0-based
                uint32_t vertex_index = 0;
                
                // Handle formats: v, v/vt, v/vt/vn, v//vn
                size_t first_slash = vertex_str.find('/');
                if (first_slash != std::string::npos) {
                    vertex_index = std::stoul(vertex_str.substr(0, first_slash)) - 1;
                } else {
                    vertex_index = std::stoul(vertex_str) - 1;
                }
                
                // Validate vertex index
                if (vertex_index >= vertices.size()) {
                    LOG_ERROR("Invalid vertex index %u at line %u in: %s (only %zu vertices defined)", 
                              vertex_index + 1, line_number, nav_file_path.c_str(), vertices.size());
                    continue;
                }
                
                polygon.push_back(vertex_index);
            }
            
            if (polygon.size() >= 3) {
                polygons.push_back(polygon);
            } else if (!polygon.empty()) {
                LOG_ERROR("Face with fewer than 3 vertices at line %u in: %s", line_number, nav_file_path.c_str());
            }
        }
    }
    
    file.close();
    
    LOG_INFO("Loaded navmesh from %s: %zu polygons, %zu vertices", 
             nav_file_path.c_str(), polygons.size(), vertices.size());
    
    return { vertices, polygons };
}

std::optional<Map> DataLoader::load_map(const std::string& map_name, const std::string& map_dir) {
    Map map;
    
    // Verify map directory exists
    if (!std::filesystem::exists(map_dir)) {
        LOG_ERROR("Map directory does not exist: %s", map_dir.c_str());
        return std::nullopt;
    }
    
    // Construct file paths
    std::string xml_path = map_dir + map_name + ".xml";
    std::string nav_path = map_dir + map_name + ".nav.obj";
    
    // Check both files exist
    if (!std::filesystem::exists(xml_path)) {
        LOG_ERROR("Map XML file not found: %s", xml_path.c_str());
        return std::nullopt;
    }
    
    if (!std::filesystem::exists(nav_path)) {
        LOG_ERROR("Map navmesh file not found: %s", nav_path.c_str());
        return std::nullopt;
    }
    
    // Load XML metadata
    pugi::xml_document doc;
    pugi::xml_parse_status status = doc.load_file(xml_path.c_str()).status;
    if (status != pugi::xml_parse_status::status_ok) {
        LOG_ERROR("Failed to parse map XML file: %s, pugixml status is %d", xml_path.c_str(), status);
        return std::nullopt;
    }
    
    pugi::xml_node map_node = doc.child("map");
    if (map_node == NULL) {
        LOG_ERROR("Map XML file missing 'map' root element: %s", xml_path.c_str());
        return std::nullopt;
    }
    
    // Extract map name
    std::string map_id = map_node.attribute("id").as_string("");
    if (map_id.empty()) {
        map_id = map_name;  // Fallback to provided map name
    }
    map.name = map_id;
    
    // Load navmesh OBJ data
    auto [vertices, polygons] = load_navmesh_obj(nav_path);
    if (vertices.empty() || polygons.empty()) {
        LOG_ERROR("Failed to load navmesh data from: %s", nav_path.c_str());
        return std::nullopt;
    }
    
    map.vertices = vertices;
    map.polygons = polygons;
    
    // Parse spawn points from XML (if they exist)
    pugi::xml_node spawn_points_node = map_node.child("spawn_points");
    if (!spawn_points_node.empty()) {
        for (pugi::xml_node spawn = spawn_points_node.child("spawn"); spawn; spawn = spawn.next_sibling("spawn")) {
            pugi::xml_node pos_node = spawn.child("position");
            if (!pos_node.empty()) {
                float x = pos_node.attribute("x").as_float(0.0f);
                float z = pos_node.attribute("z").as_float(0.0f);
                map.spawnpoints.push_back(Vec2(x, z));
                LOG_DEBUG("Loaded spawn point: (%.1f, %.1f)", x, z);
            }
        }
    }
    
    // Parse structures from XML (if they exist)
    pugi::xml_node structures_node = map_node.child("structures");
    if (!structures_node.empty()) {
        for (pugi::xml_node structure = structures_node.child("structure"); structure; structure = structure.next_sibling("structure")) {
            MapStructure struct_data;
            
            // Load attributes
            struct_data.id = structure.attribute("id").as_string("");
            struct_data.type = structure.attribute("type").as_string("");
            struct_data.team = structure.attribute("team").as_uint(0);
            
            // Load position
            pugi::xml_node pos_node = structure.child("position");
            if (!pos_node.empty()) {
                struct_data.position.x = pos_node.attribute("x").as_float(0.0f);
                struct_data.position.y = pos_node.attribute("y").as_float(0.0f);
                struct_data.position.z = pos_node.attribute("z").as_float(0.0f);
            }
            
            // Load rotation
            pugi::xml_node rot_node = structure.child("rotation");
            if (!rot_node.empty()) {
                struct_data.rotation.x = rot_node.attribute("x").as_float(0.0f);
                struct_data.rotation.y = rot_node.attribute("y").as_float(0.0f);
                struct_data.rotation.z = rot_node.attribute("z").as_float(0.0f);
            }
            
            // Load scale
            pugi::xml_node scale_node = structure.child("scale");
            if (!scale_node.empty()) {
                struct_data.scale.x = scale_node.attribute("x").as_float(1.0f);
                struct_data.scale.y = scale_node.attribute("y").as_float(1.0f);
                struct_data.scale.z = scale_node.attribute("z").as_float(1.0f);
            } else {
                struct_data.scale = Vec3(1.0f, 1.0f, 1.0f);  // Default scale to 1.0
            }
            
            map.structures.push_back(struct_data);
            LOG_DEBUG("Loaded structure '%s' of type '%s' (team %u) at (%.1f, %.1f, %.1f)", 
                     struct_data.id.c_str(), struct_data.type.c_str(), struct_data.team,
                     struct_data.position.x, struct_data.position.y, struct_data.position.z);
        }
    }
        
    // Calculate map bounds and derived size/offset from vertices
    if (!vertices.empty()) {
        float min_x = vertices[0].x, max_x = vertices[0].x;
        float min_y = vertices[0].y, max_y = vertices[0].y;
        
        for (const auto& v : vertices) {
            min_x = std::min(min_x, v.x);
            max_x = std::max(max_x, v.x);
            min_y = std::min(min_y, v.y);
            max_y = std::max(max_y, v.y);
        }
        
        map.size = Vec2(max_x - min_x, max_y - min_y);
        map.offset = Vec2((min_x + max_x) / 2.0f, (min_y + max_y) / 2.0f);
    }
    
    // Build the polygon grid for spatial acceleration
    build_polygon_grid(map);
    
    LOG_INFO("Loaded map '%s' from XML: %zu vertices, %zu polygons, %zu spawnpoints, %zu structures. Grid: %dx%d cells",
             map.name.c_str(), map.vertices.size(), map.polygons.size(), map.spawnpoints.size(),
             map.structures.size(), map.grid_width, map.grid_height);
    
    return std::optional<Map>(map);
}

std::vector<std::string> DataLoader::list_files_from_directory(std::string path, std::string file_ending) {
    std::vector<std::string> file_names;
    // Verify path exists
    if(!std::filesystem::exists(path)) {
        LOG_ERROR("DataLoader::list_files_from_directory: Path %s does not exist!", path.c_str());
        return file_names;
    }

    for(const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path)) {
        // Recursively list files in subdirectories
        if(entry.exists() && entry.is_directory()) {
            std::vector<std::string> subdirectory_files = list_files_from_directory(entry.path().string(), file_ending);
            file_names.insert(file_names.end(), subdirectory_files.begin(), subdirectory_files.end());
        }
        
        if(entry.exists() && entry.is_regular_file() && !entry.path().extension().compare(file_ending)) {
            file_names.push_back(entry.path().string());
        }
    }

    return file_names;
}

void DataLoader::build_polygon_grid(Map& map) {
    map.grid_cell_size = 1.0f; // Default cell size
    // Require at least some vertices and polygons
    if (map.vertices.empty() || map.polygons.empty()) {
        LOG_WARN("build_polygon_grid: Invalid input (vertices: %zu, polygons: %zu, cell_size: %.1f)",
                 map.vertices.size(), map.polygons.size(), map.grid_cell_size);
        return;
    }

    // Find bounds of all vertices
    Vec2 min_bounds = map.vertices[0];
    Vec2 max_bounds = map.vertices[0];
    
    for (const auto& vertex : map.vertices) {
        min_bounds.x = std::min(min_bounds.x, vertex.x);
        min_bounds.y = std::min(min_bounds.y, vertex.y);
        max_bounds.x = std::max(max_bounds.x, vertex.x);
        max_bounds.y = std::max(max_bounds.y, vertex.y);
    }
    
    // Add padding to bounds to handle edge cases
    const float PADDING = map.grid_cell_size * 0.5f;
    min_bounds.x -= PADDING;
    min_bounds.y -= PADDING;
    max_bounds.x += PADDING;
    max_bounds.y += PADDING;

    map.grid_origin = min_bounds;

    // Calculate grid dimensions
    Vec2 bounds_size = max_bounds - min_bounds;
    map.grid_width = std::max(1, static_cast<int>(std::ceil(bounds_size.x / map.grid_cell_size)));
    map.grid_height = std::max(1, static_cast<int>(std::ceil(bounds_size.y / map.grid_cell_size)));

    // Initialize grid: grid_cells[y * grid_width + x] = {polygon_ids}
    map.grid_cells.resize(map.grid_width * map.grid_height);
    
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
        
        out_x = static_cast<int>(std::floor(relative_pos.x / map.grid_cell_size));
        out_y = static_cast<int>(std::floor(relative_pos.y / map.grid_cell_size));

        // Clamp to valid grid range
        out_x = std::max(0, std::min(out_x, map.grid_width - 1));
        out_y = std::max(0, std::min(out_y, map.grid_height - 1));
    };

    // Helper lambda: point-in-polygon test using ray casting
    auto PointInPolygon = [&](const Vec2& point, const std::vector<uint32_t>& polygon) -> bool {
        if (polygon.size() < 3) return false;

        int intersections = 0;
        size_t n = polygon.size();

        for (size_t i = 0; i < n; ++i) {
            uint32_t idx1 = polygon[i];
            uint32_t idx2 = polygon[(i + 1) % n];

            if (idx1 >= map.vertices.size() || idx2 >= map.vertices.size()) {
                continue;
            }

            const Vec2& v1 = map.vertices[idx1];
            const Vec2& v2 = map.vertices[idx2];

            // Check if horizontal ray from point intersects edge
            if ((v1.y <= point.y && point.y < v2.y) || (v2.y <= point.y && point.y < v1.y)) {
                // Compute x-intersection of ray with edge
                float x_intersect = v1.x + (point.y - v1.y) * (v2.x - v1.x) / (v2.y - v1.y);
                if (point.x < x_intersect) {
                    intersections++;
                }
            }
        }

        return intersections % 2 == 1;
    };

    // Add each polygon to grid cells it overlaps
    for (uint32_t poly_id = 0; poly_id < map.polygons.size(); ++poly_id) {
        Vec2 poly_min, poly_max;
        GetPolygonBounds(map.polygons[poly_id], poly_min, poly_max);

        // Convert polygon bounds to grid coordinates
        int min_x, min_y, max_x, max_y;
        WorldToGridCoords(poly_min, min_x, min_y);
        WorldToGridCoords(poly_max, max_x, max_y);

        // Add polygon ID to grid cells only if the center of the cell is inside or touches the polygon
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                // Calculate cell center in world coordinates
                Vec2 cell_center = map.grid_origin + Vec2(
                    (x + 0.5f) * map.grid_cell_size,
                    (y + 0.5f) * map.grid_cell_size
                );

                // Only add polygon to this cell if its center is inside the polygon
                if (PointInPolygon(cell_center, map.polygons[poly_id])) {
                    int index = y * map.grid_width + x;
                    map.grid_cells[index].push_back(poly_id);
                }
            }
        }
    }

    // Log statistics
    size_t total_entries = 0;
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

