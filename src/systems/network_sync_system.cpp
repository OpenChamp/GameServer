#include "network_sync_system.hpp"
#include "serialization_system.hpp"
#include "components/movement.hpp"
#include "components/entity_state.hpp"
#include "components/network_entity.hpp"
#include <log.hpp>

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
        
        // Serialize entity update
        bool send_full_state = force_full_sync_next_frame_ || net_comp->force_full_sync_next_frame || is_first_sync;
        std::vector<uint8_t> packet = serialize_entity_update(entity->get_id(), *entity, send_full_state);
        
        if (!packet.empty()) {
            ctx.network_service->broadcast_packet(packet);
            
            // Update last synced state
            auto* move = entity->get_component<Movement>();
            auto* state = entity->get_component<EntityStateComponent>();
            
            Vec2 pos = move ? move->position : Vec2(0, 0);
            EntityState state_val = state ? state->current_state : EntityState::SPAWNED;
            
            net_comp->mark_synced(pos, state_val, 0, current_frame_);
            LOG_DEBUG("Synced entity %u (is_first_sync=%s) at position (%.1f, %.1f), state=%d", 
                     entity->get_id(), is_first_sync ? "true" : "false", pos.x, pos.y, (int)state_val);
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

std::vector<uint8_t> NetworkSyncSystem::serialize_entity_update(EntityID entity_id, const Entity& entity, 
                                                                  bool send_full_state) const {
    std::vector<uint8_t> packet;
    
    // Get entity components
    auto* move = entity.get_component<Movement>();
    auto* state = entity.get_component<EntityStateComponent>();
    auto* net_comp = entity.get_component<NetworkEntityComponent>();
    
    if (!move || !state) {
        if (!move) {
            LOG_WARN("Entity %u missing Movement component", entity_id);
        }
        if (!state) {
            LOG_WARN("Entity %u missing EntityStateComponent", entity_id);
        }
        return packet;  // Return empty packet
    }
    
    // Use existing serialization system for now
    // TODO: Implement delta compression for bandwidth optimization
    if (send_full_state || !net_comp) {
        // Full state sync - use existing serialization
        packet = SerializationSystem::serialize_entity_position(entity_id, move->position);
    } else {
        // Delta sync - only position for now
        if (net_comp->has_position_changed(move->position)) {
            packet = SerializationSystem::serialize_entity_position(entity_id, move->position);
        }
    }
    
    return packet;
}
