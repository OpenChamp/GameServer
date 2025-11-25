#pragma once

#include "component.hpp"
#include "movement.hpp"
#include "entity_state.hpp"
#include "stats.hpp"
#include <cstdint>
#include <optional>

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
    std::optional<Stats> last_synced_stats;         // Stats at last sync (for change detection)
    
    // === Bandwidth Optimization ===
    static constexpr uint32_t MIN_POSITION_CHANGE = 1;  // Only sync if moved 1 unit
    static constexpr uint32_t SYNC_FRAME_INTERVAL = 1;      // Sync every frame
    
    COMPONENT_TYPE_ID(NetworkEntityComponent, 3004)
};
