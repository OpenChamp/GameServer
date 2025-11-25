#include <systems/data_loader.hpp>

#include <filesystem>

#include <libs/pugixml.hpp>
#include <component_registry.hpp>
#include <components/stats.hpp>
#include <components/network_entity.hpp>
#include <components/entity_state.hpp>
#include <components/npc_component.hpp>
#include <components/auto_attack.hpp>
#include <components/target.hpp>
#include <components/attack.hpp>

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
 * Convert string to AIPersonality enum.
 * @param str The string to convert (case-insensitive)
 * @return AIPersonality enum value, defaults to BALANCED if unrecognized
 */
static AutoAttackComponent::AIPersonality string_to_ai_personality(const std::string& str) {
    if (str == "passive") {
        return AutoAttackComponent::AIPersonality::PASSIVE;
    } else if (str == "defensive") {
        return AutoAttackComponent::AIPersonality::DEFENSIVE;
    } else if (str == "aggressive") {
        return AutoAttackComponent::AIPersonality::AGGRESSIVE;
    } else if (str == "zealous") {
        return AutoAttackComponent::AIPersonality::ZEALOUS;
    }
    // Default to BALANCED for unrecognized or empty strings
    return AutoAttackComponent::AIPersonality::BALANCED;
}

/**
 * Convert string to TargetPriority enum.
 * @param str The string to convert (case-insensitive)
 * @return TargetPriority enum value, defaults to NEAREST if unrecognized
 */
static TargetComponent::TargetPriority string_to_target_priority(const std::string& str) {
    if (str == "lowest_health") {
        return TargetComponent::TargetPriority::LOWEST_HEALTH;
    } else if (str == "highest_threat") {
        return TargetComponent::TargetPriority::HIGHEST_THREAT;
    } else if (str == "highest_damage") {
        return TargetComponent::TargetPriority::HIGHEST_DAMAGE;
    }
    // Default to NEAREST for unrecognized or empty strings
    return TargetComponent::TargetPriority::NEAREST;
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
        LOAD_ATTRIBUTE(npc_node, npc, chase_distance, float)
        temp.component_templates.push_back(npc);
    }

    pugi::xml_node auto_attack_node = rootNode.child("auto_attack");
    if(!auto_attack_node.empty()) {
        std::shared_ptr<AutoAttackComponent> auto_attack = std::make_shared<AutoAttackComponent>();
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, enabled, bool)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, aggressive, bool)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, retaliate_only, bool)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, aggression_range, float)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, retreat_range, float)
        LOAD_ENUM_ATTRIBUTE(auto_attack_node, auto_attack, personality, string_to_ai_personality)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, combat_timeout_ms, float)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, base_damage_multiplier, float)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, bonus_damage, float)
        LOAD_ENUM_ATTRIBUTE(auto_attack_node, auto_attack, damage_type, string_to_damage_type)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, attack_animation_duration_ms, float)
        LOAD_ATTRIBUTE(auto_attack_node, auto_attack, hit_timing_percent, float)
        temp.component_templates.push_back(auto_attack);
    }

    pugi::xml_node target_node = rootNode.child("target");
    if(!target_node.empty()) {
        std::shared_ptr<TargetComponent> target = std::make_shared<TargetComponent>();
        LOAD_ENUM_ATTRIBUTE(target_node, target, targeting_mode, string_to_target_priority)
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