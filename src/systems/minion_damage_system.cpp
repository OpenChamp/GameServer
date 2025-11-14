#include "minion_damage_system.hpp"
#include "components/minion.hpp"
#include "components/stats.hpp"
#include <log.hpp>

std::vector<EntityID> MinionDamageSystem::update(EntityManager& entity_manager) {
    std::vector<EntityID> dead_minions;
    
    // Iterate through all minions
    std::vector<Entity*> minions = entity_manager.get_entities_with_component<Minion>();
    for (Entity* entity : minions) {
        if (!entity->has_component<Stats>()) {
            continue;
        }
        
        Minion* minion = entity->get_component<Minion>();
        Stats* stats = entity->get_component<Stats>();
        
        // Skip if already dead
        if (minion->is_dead) {
            continue;
        }
        
        // Apply damage if reached destination
        if (minion->has_reached_destination && !minion->is_dead) {
            stats->health -= DESTINATION_DAMAGE;
            
            LOG_INFO("Minion (ID %u) reached destination and took %.1f damage (health: %.1f/%.1f)",
                     entity->get_id(), DESTINATION_DAMAGE, stats->health, stats->max_health);
            
            // Check if dead
            if (stats->health <= 0.0f) {
                minion->is_dead = true;
                stats->health = 0.0f;
                dead_minions.push_back(entity->get_id());
                
                LOG_INFO("Minion (ID %u) died", entity->get_id());
            }
        }
    }
    
    return dead_minions;
}
