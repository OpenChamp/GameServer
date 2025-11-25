#include <systems/core/spawning_system.hpp>
#include <components/movement.hpp>
#include <components/stats.hpp>
#include <components/network_entity.hpp>
#include <components/entity_state.hpp>
#include <systems/util/serialization_system.hpp>
#include <services/network_service.hpp>
#include <libs/log.hpp>

EntityID SpawningSystem::spawn_entity_from_template(const SystemContext& ctx,
                                                    const std::string& template_id,
                                                    const Vec2& position,
                                                    uint8_t team_id) {
    // Create entity from template
    Entity& entity = ctx.entity_manager.create_entity_from_template(template_id);
    EntityID entity_id = entity.get_id();
    
    if (entity_id == INVALID_ENTITY_ID) {
        LOG_ERROR("Failed to spawn entity from template: %s", template_id.c_str());
        return INVALID_ENTITY_ID;
    }
    
    // Set position
    auto* move = entity.get_component<Movement>();
    if (move) {
        move->position = position;
    }
    
    // Set team ID
    auto* stats = entity.get_component<Stats>();
    if (stats) {
        stats->team_id = team_id;
        LOG_INFO("Spawned minion (ID %u) with health=%f, max_health=%f, team=%u", 
                 entity_id, stats->health, stats->max_health, team_id);
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
    
    // Broadcast spawn to clients - NOTE: NetworkSyncSystem will handle first sync
    // The entity position will be sent in the same frame via NetworkSyncSystem
    broadcast_entity_spawn(ctx, entity_id, position, team_id, template_id);
    
    LOG_INFO("Spawned entity (ID %u) from template '%s' at (%.1f, %.1f), team %u",
             entity_id, template_id.c_str(), position.x, position.y, team_id);
    
    return entity_id;
}

void SpawningSystem::broadcast_entity_spawn(const SystemContext& ctx,
                                           EntityID entity_id,
                                           const Vec2& position,
                                           uint8_t team_id,
                                           const std::string& template_id) const {
    if (!ctx.network_service) {
        return;
    }
    
    // Serialize and broadcast entity spawn packet
    std::vector<uint8_t> packet = SerializationSystem::serialize_entity_spawn(entity_id, position, team_id, template_id);
    if (!packet.empty()) {
        ctx.network_service->broadcast_packet(packet);
    }
}
