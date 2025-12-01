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
    
    // Find collision-free spawn position
    Vec2 spawn_position = CollisionSystem::find_free_space(position, collision_radius, const_cast<EntityManager&>(ctx.entity_manager));
    
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
    // Team 1 moves toward spawnpoint 1, Team 2 moves toward spawnpoint 0 (opposite direction)
    auto* npc = entity.get_component<NPCComponent>();
    if (npc && npc->npc_type == NPCType::MINION) {
        npc->objective = (team_id == 1) ? Vec2(0, 28) : Vec2(0, -28);
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

