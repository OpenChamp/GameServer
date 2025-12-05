#include <systems/core/spawning_system.hpp>
#include <systems/core/collision_system.hpp>
#include <components/movement.hpp>
#include <components/stats.hpp>
#include <components/network_entity.hpp>
#include <components/entity_state.hpp>
#include <components/intent.hpp>
#include <components/npc_component.hpp>
#include <libs/log.hpp>

EntityID SpawningSystem::spawn_entity_from_template(const SystemContext& ctx,
                                                    const std::string& template_id,
                                                    const Vec2& position,
                                                    uint8_t team_id) {
    // Get collision radius from template to find free space
    float collision_radius = ctx.entity_manager.get_template_collision_radius(template_id);
    
    // Find collision-free spawn position within grid bounds
    Vec2 spawn_position = CollisionSystem::find_free_space(position, collision_radius, const_cast<EntityManager&>(ctx.entity_manager), ctx.map);
    
    // Create entity from template
    Entity& entity = ctx.entity_manager.create_entity_from_template(template_id);
    EntityID entity_id = entity.get_id();
    
    if (entity_id == INVALID_ENTITY_ID) {
        LOG_ERROR("Failed to spawn entity from template: %s", template_id.c_str());
        return INVALID_ENTITY_ID;
    }
    
    // Set position (collision-checked position)
    auto* move = entity.get_component<Movement>();
    if (move) {
        move->position = spawn_position;
    }
    
    // Set team ID
    auto* stats = entity.get_component<Stats>();
    if (stats) {
        stats->team_id = team_id;
        LOG_INFO("Spawned minion (ID %u) with health=%f, max_health=%f, team=%u", 
                 entity_id, stats->health, stats->max_health, team_id);
    }
    // Adjust intent objective target based on team
    // Minions should target the enemy core (using structure data from map)
    auto* npc = entity.get_component<NPCComponent>();
    if (npc && npc->npc_type == NPCType::MINION) {
        // Find enemy core position from map structures
        Vec2 enemy_core_position = Vec2(0, 0);  // Default fallback
        
        if (ctx.map) {
            uint8_t enemy_team = (team_id == 1) ? 2 : 1;
            
            // Search for enemy core in map structures
            for (const auto& structure : ctx.map->structures) {
                if (structure.type == "core" && structure.team == enemy_team) {
                    // Convert 3D position to 2D (use X and Z, ignore Y)
                    enemy_core_position = Vec2(structure.position.x, structure.position.z);
                    LOG_DEBUG("Found enemy core for team %u at position (%.1f, %.1f)", 
                             team_id, enemy_core_position.x, enemy_core_position.y);
                    break;
                }
            }
            
            if (enemy_core_position.x == 0.0f && enemy_core_position.y == 0.0f) {
                LOG_WARN("Could not find enemy core for team %u in %zu structures. Minion %u will default to (0,0)",
                         enemy_team, ctx.map->structures.size(), entity_id);
            }
        } else {
            LOG_ERROR("Map context is null! Minion %u objective cannot be set properly", entity_id);
        }
        
        npc->objective = enemy_core_position;
        LOG_DEBUG("Entity %u (team %u): Set objective target to <%f, %f>", 
                 entity_id, team_id, npc->objective.x, npc->objective.y);
    }
    
    
    // Add network entity component for syncing
    if (!entity.has_component<NetworkEntityComponent>()) {
        auto net_comp = std::make_unique<NetworkEntityComponent>();
        net_comp->force_full_sync_next_frame = true;  // Ensure initial spawn is sent to clients
        entity.add_component(std::move(net_comp));
    }
    
    // Add entity state component if not present
    if (!entity.has_component<EntityStateComponent>()) {
        auto entity_state = std::make_unique<EntityStateComponent>();
        entity_state->current_state = EntityState::SPAWNED;
        entity.add_component(std::move(entity_state));
    }
    
    LOG_INFO("Spawned entity (ID %u) from template '%s' at (%.1f, %.1f), team %u",
             entity_id, template_id.c_str(), spawn_position.x, spawn_position.y, team_id);
    
    // NetworkSyncSystem will broadcast spawn to clients on first sync (when last_sync_frame == 0)
    // This centralizes all network communication through the SyncManager
    
    return entity_id;
}

