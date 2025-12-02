#pragma once

#include <vector>
#include <string>
#include <map>
#include <optional>

#include <component_registry.hpp>
#include <components/movement.hpp>
#include <components/pathfinding.hpp>
#include <components/stats.hpp>
#include <components/map.hpp>
#include <libs/log.hpp>
#include <libs/math.hpp>

/**
 * Template for creating new entities.
 */
class EntityTemplate {
public:
    std::string id;
    std::vector<std::shared_ptr<Component>> component_templates;
};

/**
 * Loads data (e.g. entities, maps) from xml and binary files
 */
class DataLoader {
public:
    /**
     * Create a new entity tempalte based on data from a file.
     * @param file_name The name of the file from which to load the entity template
     * @return The newly created entity template
     */
    static EntityTemplate load_entity_template(std::string file_name);

    /**
     * Load a map from XML and .nav files.
     * @param map_name The name of the map (e.g., "konda")
     * @param map_dir The directory containing map files (default: "./data/maps/")
     * @return Optional Map component, or empty if loading failed
     */
    static std::optional<Map> load_map(const std::string& map_name, const std::string& map_dir = "./data/maps/");

    /**
     * List all files with a matching file ending in a directory.
     * @param path The directory to search
     * @param file_ending The file ending to match for, e.g. ".xml"
     * @return A vector of file names found in the directory, e.g. "path/file-name.xml"
     */
    static std::vector<std::string> list_files_from_directory(std::string path, std::string file_ending);

private:
    /**
     * Load navmesh data from a .nav.obj OBJ file.
     * Parses standard OBJ vertex (v) and face (f) definitions.
     * @param nav_file_path Path to the .nav.obj file
     * @return A pair of (vertices, polygons), empty if loading failed
     */
    static std::pair<std::vector<Vec2>, std::vector<std::vector<uint32_t>>> load_navmesh_obj(const std::string& nav_file_path);
};