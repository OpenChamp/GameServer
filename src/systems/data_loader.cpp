#include <systems/data_loader.hpp>

#include <filesystem>

#include <libs/pugixml.hpp>

/**
 * Convenience macro. Loads the attribute from the node if the node has it, and sets it in the component.
 * @param node A pugi::xml_node which (maybe) has the attribute
 * @param comp The Component in which to set the value of the attribute
 * @param attr The attribute to load. Must match exactly both the attribute name in the xml and the property in the Component.
 * @param as pugixml can load attributes as a number of different types, for example string, float or int
 */
#define LOAD_ATTRIBUTE(node, comp, attr, as)                   \
    {                                                          \
        pugi::xml_attribute loaded = ##node.attribute(#attr);  \
        if(!loaded.empty()) {                                  \
            ##comp->##attr = loaded.as_##as();                 \
        }                                                      \
    }


EntityTemplate DataLoader::load_entity_template(std::string file_name) {
    EntityTemplate temp;

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
    if(movementNode != NULL) {
        std::shared_ptr<Movement> movement = std::make_shared<Movement>();
        LOAD_ATTRIBUTE(movementNode, movement, move_speed, float)
        temp.component_templates.push_back(movement);
    }
    
    pugi::xml_node pathfindingNode = rootNode.child("pathfinding");
    if(pathfindingNode != NULL) {
        std::shared_ptr<PathfindingComponent> pathfinding = std::make_shared<PathfindingComponent>();
        temp.component_templates.push_back(pathfinding);
    }

    pugi::xml_node statsNode = rootNode.child("stats");
    if(statsNode != NULL) {
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

        temp.component_templates.push_back(stats);
    }

    LOG_INFO("Successfully loaded template entity \"%s\" from %s with %d components", temp.id.c_str(), file_name.c_str(), temp.component_templates.size());
    return temp;
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