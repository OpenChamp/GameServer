#pragma once

#include "component.hpp"
#include "movement.hpp"
#include "entity_state.hpp"
#include <cstdint>

/**
 * NetworkEntityComponent - Tracks network synchronization state
 * 
 * USAGE: Add to entities that need to be synchronized to clients.
 *        Presence of this component means the entity should always be synced.
 * SYSTEMS: NetworkSyncSystem
 * 
 * PURPOSE:
 *   - Cache last synced state to detect changes
 *   - Optimize bandwidth by only sending changed data
 *   - Track sync state per entity
 * 
 * BENEFITS:
 *   - Efficient change detection (position, state, custom properties)
 *   - Per-entity sync control via component presence
 *   - Easy to extend with new sync requirements
 * 
 * EXAMPLE:
 *   // Check if entity has changed since last sync
 *   if (entity.get_component<NetworkEntityComponent>()->has_changed(entity)) {
 *       network_sync_system.sync_entity(entity_id);
 *   }
 */
struct NetworkEntityComponent : public Component {
    // === Sync Control ===
    bool force_full_sync_next_frame = false;        // Send all data next frame, not just deltas
    
    // === Sync Tracking ===
    uint32_t last_sync_frame = 0;                   // Frame number when last synced
    
    // === Last Known State (for delta detection) ===
    Vec2 last_synced_position = Vec2(0.0f, 0.0f);  // Position at last sync
    EntityState last_synced_state = EntityState::SPAWNED;  // State at last sync
    uint32_t last_synced_health = 0;                // Health at last sync (if applicable)
    
    // === Bandwidth Optimization ===
    static constexpr uint32_t MIN_POSITION_CHANGE = 1;  // Only sync if moved 1 unit
    static constexpr uint32_t SYNC_FRAME_INTERVAL = 1;      // Sync every frame
    
    /**
     * Check if entity position has changed significantly since last sync.
     * @param current_position Current entity position
     * @return true if position change is significant
     */
    bool has_position_changed(const Vec2& current_position) const {
        float dx = current_position.x - last_synced_position.x;
        float dy = current_position.y - last_synced_position.y;
        float dist_sq = dx * dx + dy * dy;
        return dist_sq >= (MIN_POSITION_CHANGE * MIN_POSITION_CHANGE);
    }
    
    /**
     * Check if entity state has changed since last sync.
     * @param current_state Current entity state
     * @return true if state has changed
     */
    bool has_state_changed(EntityState current_state) const {
        return current_state != last_synced_state;
    }
    
    /**
     * Mark this entity as fully synced with the given state.
     * @param position Current position
     * @param state Current state
     * @param health Current health (optional)
     * @param current_frame Frame number
     */
    void mark_synced(const Vec2& position, EntityState state, uint32_t health, uint32_t current_frame) {
        last_synced_position = position;
        last_synced_state = state;
        last_synced_health = health;
        last_sync_frame = current_frame;
        force_full_sync_next_frame = false;
    }
    
    COMPONENT_TYPE_ID(NetworkEntityComponent, 3004)
};
