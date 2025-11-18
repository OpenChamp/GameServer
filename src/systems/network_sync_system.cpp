#include "network_sync_system.hpp"
#include "serialization_system.hpp"
#include "components/movement.hpp"
#include "components/entity_state.hpp"
#include "components/network_entity.hpp"
#include "components/stats.hpp"
#include <log.hpp>

// Helper function to compute a simple hash of stats for change detection
static uint32_t compute_stats_hash(const Stats& stats) {
    // Simple hash combining critical stats
    // Using bit shifts and XOR to mix the values
    uint32_t hash = 0;
    
    // Hash health and max_health (important for gameplay)
    uint32_t health_bits = static_cast<uint32_t>(stats.health * 100.0f);
    uint32_t max_health_bits = static_cast<uint32_t>(stats.max_health * 100.0f);
    hash ^= health_bits;
    hash ^= (max_health_bits << 8);
    
    // Hash mana and max_mana
    uint32_t mana_bits = static_cast<uint32_t>(stats.mana * 100.0f);
    uint32_t max_mana_bits = static_cast<uint32_t>(stats.max_mana * 100.0f);
    hash ^= (mana_bits << 16);
    hash ^= (max_mana_bits << 24);
    
    // Hash level
    hash ^= stats.level;
    
    return hash;
}

void NetworkSyncSystem::update(const SystemContext& ctx) {
    if(current_frame_ % 30 == 0) {
        LOG_INFO("NetworkSyncSystem::update - Frame %u", current_frame_);
    }

    if (!ctx.network_service || !ctx.has_network_service()) {
        return;
    }
    
    current_frame_++;
    
    // Get all Network Entities
    auto network_entities = ctx.entity_manager.get_entities_with_component<NetworkEntityComponent>();
    
    for (auto* entity : network_entities) {
        if (!entity) continue;
        
        auto* net_comp = entity->get_component<NetworkEntityComponent>();
        if (!net_comp) {
            continue;  // Should never happen since we queried for this component
        }
        
        // For newly spawned entities (last_sync_frame == 0), always sync
        bool is_first_sync = (net_comp->last_sync_frame == 0);
        
        // Check if this frame should sync this entity
        if (!is_first_sync && !force_full_sync_next_frame_ && 
            (current_frame_ - net_comp->last_sync_frame) < SYNC_INTERVAL) {
            LOG_DEBUG("Skipping sync for entity %u (frame %u)", entity->get_id(), current_frame_);
            continue;  // Skip this frame for this entity
        }
        
        // Check if entity has actually changed
        if (!has_entity_changed(*entity, current_frame_) && !force_full_sync_next_frame_ && !is_first_sync) {
            LOG_DEBUG("Skipping sync for entity %u (frame %u)", entity->get_id(), current_frame_);
            continue;  // No changes to sync
        }

        if(is_first_sync) {
            Vec2 spawn_position = Vec2(0, 0);
            if(Movement* movement = entity->get_component<Movement>()) {
                spawn_position = movement->position;
            }
            // TODO what to do with team if there is no stats component? - ploinky 17/11/2025
            uint8_t team_id = 0;
            if(Stats* stats = entity->get_component<Stats>()) {
                team_id = stats->team_id;
            }
            std::string template_id = "";
            if(TemplateComponent* templateComponent = entity->get_component<TemplateComponent>()) {
                template_id = templateComponent->template_id;
            }
            ctx.network_service->broadcast_packet(SerializationSystem::serialize_entity_spawn(entity->get_id(), spawn_position, team_id, template_id));
        }
        
        // Serialize entity update - returns vector of packets (one per changed component)
        bool send_full_state = force_full_sync_next_frame_ || net_comp->force_full_sync_next_frame || is_first_sync;
        std::vector<std::vector<uint8_t>> packets = serialize_entity_update(entity->get_id(), *entity, send_full_state);
        
        if (!packets.empty()) {
            // Broadcast all component update packets
            // TODO: Implement Fog of War to avoid sending packets to unseen clients
            for (const auto& packet : packets) {
                if (!packet.empty()) {
                    ctx.network_service->broadcast_packet(packet);
                }
            }
            
            // Update last synced state
            auto* move = entity->get_component<Movement>();
            auto* state = entity->get_component<EntityStateComponent>();
            auto* stats = entity->get_component<Stats>();
            
            Vec2 pos = move ? move->position : Vec2(0, 0);
            EntityState state_val = state ? state->current_state : EntityState::SPAWNED;
            uint32_t stats_hash = stats ? compute_stats_hash(*stats) : 0;
            
            net_comp->mark_synced(pos, state_val, stats_hash, current_frame_);
            LOG_DEBUG("Synced entity %u (is_first_sync=%s) at position (%.1f, %.1f), state=%d, packets=%zu", 
                     entity->get_id(), is_first_sync ? "true" : "false", pos.x, pos.y, (int)state_val, packets.size());
        } else {
            LOG_WARN("No data serialized for entity %u during sync", entity->get_id());
        }
    }
    
    // Clear forced sync flag after processing
    force_full_sync_next_frame_ = false;
}

bool NetworkSyncSystem::has_entity_changed(const Entity& entity, uint32_t current_frame) const {
    auto* net_comp = entity.get_component<NetworkEntityComponent>();
    if (!net_comp) {
        return false;
    }
    
    auto* state = entity.get_component<EntityStateComponent>();
    auto* move = entity.get_component<Movement>();
    
    // Always sync entities in MOVING state
    if (state && state->current_state == EntityState::MOVING) {
        return true;
    }
    
    // Check position change
    if (move && net_comp->has_position_changed(move->position)) {
        return true;
    }
    
    // Check state change (for state transitions)
    if (state && net_comp->has_state_changed(state->current_state)) {
        return true;
    }
    
    // Force sync if requested
    if (net_comp->force_full_sync_next_frame) {
        return true;
    }
    
    // Debug: log why entity isn't changing (first 50 frames only)
    static uint32_t logged_frames = 0;
    if (logged_frames < 50 && state) {
        LOG_INFO("Frame %u: Entity %u state=%d, sync_check: moving=%s, pos_changed=%s, state_changed=%s, force=%s",
                 current_frame, entity.get_id(), (int)state->current_state,
                 (state->current_state == EntityState::MOVING) ? "yes" : "no",
                 move && net_comp->has_position_changed(move->position) ? "yes" : "no",
                 state && net_comp->has_state_changed(state->current_state) ? "yes" : "no",
                 net_comp->force_full_sync_next_frame ? "yes" : "no");
        logged_frames++;
    }
    
    return false;
}

std::vector<std::vector<uint8_t>> NetworkSyncSystem::serialize_entity_update(EntityID entity_id, const Entity& entity, 
                                                                               bool send_full_state) const {
    std::vector<std::vector<uint8_t>> packets;
    
    // Get entity components
    auto* move = entity.get_component<Movement>();
    auto* state = entity.get_component<EntityStateComponent>();
    auto* stats = entity.get_component<Stats>();
    auto* net_comp = entity.get_component<NetworkEntityComponent>();
    
    if (!move || !state) {
        if (!move) {
            LOG_WARN("Entity %u missing Movement component", entity_id);
        }
        if (!state) {
            LOG_WARN("Entity %u missing EntityStateComponent", entity_id);
        }
        return packets;  // Return empty vector
    }
    
    // ========================================================================
    // Position Update Packet
    // ========================================================================
    if (send_full_state || !net_comp) {
        // Full state sync - always send position
        packets.push_back(SerializationSystem::serialize_entity_position(entity_id, move->position));
    } else {
        // Delta sync - only if position changed
        if (net_comp->has_position_changed(move->position)) {
            packets.push_back(SerializationSystem::serialize_entity_position(entity_id, move->position));
        }
    }
    
    // ========================================================================
    // Stats Update Packet (Health, Mana, Level)
    // ========================================================================
    if (stats) {
        if (send_full_state || !net_comp) {
            // Full state sync - send health/mana/level
            packets.push_back(SerializationSystem::serialize_entity_stats(entity_id, *stats));
        } else {
            // Delta sync - check if stats changed using generic hash comparison
            uint32_t current_stats_hash = compute_stats_hash(*stats);
            if (net_comp->has_stats_changed(current_stats_hash)) {
                packets.push_back(SerializationSystem::serialize_entity_stats(entity_id, *stats));
            }
        }
    }
    
    return packets;
}
