#pragma once

#include <vector>
#include <string>
#include <map>

#include <components/movement.hpp>
#include <components/stats.hpp>
#include <libs/log.hpp>

/**
 * Template for creating new entities.
 */
class EntityTemplate {
public:
    std::string id;
    std::vector<std::shared_ptr<Component>> component_templates;
};

/**
 * Loads data (e.g. entities) from xml files
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
     * List all files with a matching file ending in a directory.
     * @param path The directory to search
     * @param file_ending The file ending to match for, e.g. ".xml"
     * @return A vector of file names found in the directory, e.g. "path/file-name.xml"
     */
    static std::vector<std::string> list_files_from_directory(std::string path, std::string file_ending);
};