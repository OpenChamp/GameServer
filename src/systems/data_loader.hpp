#pragma once

#include <systems/entity_manager.hpp>

#include <components/movement.hpp>
#include <components/stats.hpp>

#define LOAD_ATTRIBUTE(node, comp, attr, as)                   \
    {                                                          \
        pugi::xml_attribute loaded = ##node.attribute(#attr);  \
        if(!loaded.empty()) {                                  \
            ##comp->##attr = loaded.as_##as();                 \
        }                                                      \
    }

/**
 * Loads data (e.g. entities) from xml files
 */
class DataLoader {
public:
    /**
     * Create a new entity base on data from a file.
     * @param entity_type_id Type-id of the entity. The entity will be loaded from data/entities/`{entity_type_id}`.xml
     * @return Reference to the newly created entity
     */
    static Entity& create_entity_from_data(EntityManager* entity_manager, const char* type_id) {
        Entity& ent = entity_manager->create_entity();

        // TODO should probably not let pugi load the file for us; thinking .pak files etc... - ploinky 14/11/2025
        pugi::xml_document doc;
        
        size_t file_name_length = strnlen_s(type_id, 1024) + 9 + 1; // "data/" + type_id + ".xml" + '\0'
        char* const file_name = (char*) malloc(file_name_length);
        snprintf(file_name, file_name_length, "data/%s.xml", type_id);

        pugi::xml_parse_status status = doc.load_file(file_name).status;
        if(status != pugi::xml_parse_status::status_ok) {
            LOG_ERROR("Failed to load entity from data file: %s, pugixml status is %d", type_id, status);
            return ent;
        }

        pugi::xml_node rootNode = doc.child("entity");
        if(rootNode == NULL) {
            LOG_ERROR("Failed to load entity from data file: %s, rootNode 'entity' is missing", type_id);
            return ent;
        }

        pugi::xml_node movementNode = rootNode.child("movement");
        if(movementNode != NULL) {
            std::unique_ptr<Movement> movement = std::make_unique<Movement>();
            LOAD_ATTRIBUTE(movementNode, movement, move_speed, float)
            ent.add_component(std::move(movement));
        }

        pugi::xml_node statsNode = rootNode.child("stats");
        if(statsNode != NULL) {
            std::unique_ptr<Stats> stats = std::make_unique<Stats>();

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

            ent.add_component(std::move(stats));
        }

        return ent;
    }

private:
};