#pragma once

#include <systems/entity_manager.hpp>
#include <systems/system_context.hpp>
#include <services/network_service.hpp>
#include <vector>
#include <cstdint>

/**
 * NetworkSyncSystem - Synchronizes entity state to connected clients
 * 
 * RESPONSIBILITIES:
 *   - Detect which entities changed this frame (position, state, etc.)
 *   - Serialize changed data into network packets
 *   - Broadcast updates to all clients who can see the entity
 *   - Track last-synced state for each entity
 *   - Optimize bandwidth by sending only changed data
 * 
 * OPTIMIZATIONS:
 *   - Delta compression: Only sends changed components, not full state
 *   - Frame rate limiting: Syncs every N frames instead of every frame
 *   - Position thresholding: Only syncs if entity moved significant distance
 *   - Network visibility: Respects NetworkEntityComponent visibility flags
 *   - Batching: Multiple entity updates per packet
 * 
 * COMPONENTS USED:
 *   - Movement (position, velocity)
 *   - EntityStateComponent (current state: spawned, alive, dead, etc.)
 *   - NetworkEntityComponent (sync tracking, visibility, change detection)
 * 
 * USAGE:
 *   SystemContext ctx{...};
 *   network_sync_system.update(ctx);
 */
class NetworkSyncSystem {
public:
    NetworkSyncSystem() = default;
    ~NetworkSyncSystem() = default;
    
    // Prevent copying
    NetworkSyncSystem(const NetworkSyncSystem&) = delete;
    NetworkSyncSystem& operator=(const NetworkSyncSystem&) = delete;
    
    // Allow moving
    NetworkSyncSystem(NetworkSyncSystem&&) = default;
    NetworkSyncSystem& operator=(NetworkSyncSystem&&) = default;
    
    /**
     * Update and broadcast entity state to all clients.
     * 
     * Process:
     *   1. Iterate all entities with NetworkEntityComponent
     *   2. Check if entity is visible and has changed
     *   3. Serialize changed data
     *   4. Broadcast to network service
     *   5. Update last-synced state in NetworkEntityComponent
     * 
     * @param ctx System context with entity manager and network service
     */
    void update(const SystemContext& ctx);
    
    /**
     * Force next sync to send all entity data (not just deltas).
     * Useful when clients disconnect/reconnect or state becomes inconsistent.
     */
    void force_full_sync() {
        force_full_sync_next_frame_ = true;
    }

private:
    uint32_t current_frame_ = 0;                    // Frame counter for sync interval checking
    bool force_full_sync_next_frame_ = false;       // Flag to force delta-free sync
    
    // Optimization: only sync every N frames for non-critical entities
    static constexpr uint32_t SYNC_INTERVAL = 1;  // Every frame for responsive movement
    
    /**
     * Check if an entity has changed since last sync.
     * Considers position, state, and health.
     * @param entity Entity to check
     * @param current_frame Current frame number
     * @return true if position, state, or other tracked data has changed
     */
    bool has_entity_changed(const Entity& entity, uint32_t current_frame) const;
    
    /**
     * Serialize changed entity data into multiple packets (one per component type).
     * Only includes data that has actually changed (delta compression).
     * 
     * Returns one packet per component that changed:
     *   - Position packet if entity moved
     *   - Stats packet if health/mana/level changed
     *   - Additional packets as new components are added
     * 
     * @param entity_id ID of entity that changed
     * @param entity The entity
     * @param send_full_state If true, sends all data instead of just changes
     * @return Vector of serialized packets, one per changed component
     */
    std::vector<std::vector<uint8_t>> serialize_entity_update(EntityID entity_id, const Entity& entity, 
                                                               bool send_full_state) const;
};
