#pragma once

#include <components/network_entity.hpp>
#include <components/network_metadata.hpp>
#include <components/target.hpp>
#include <components/stats.hpp>
#include <components/movement.hpp>
#include <components/entity_state.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>

/**
 * ComponentUtility - Pure utility functions for component logic.
 * 
 * ECS PRINCIPLE: Components are pure data containers.
 * Systems query and modify components, but the logic belongs in systems/utilities.
 * 
 * This utility provides helper functions to replace methods that were
 * previously on components, following ECS principles strictly.
 */
class ComponentUtility {
public:
    // ========================================================================
    // NetworkMetadataComponent Utilities
    // ========================================================================
    
    /**
     * Check if player connection is stale (no activity for timeout_ms).
     * @param metadata NetworkMetadataComponent to check
     * @param timeout_ms Timeout in milliseconds (default: 30 seconds)
     * @return true if last activity exceeds timeout
     */
    static bool is_metadata_stale(const NetworkMetadataComponent& metadata, 
                                   unsigned int timeout_ms = 30000) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - metadata.last_activity);
        return elapsed.count() > (long long)timeout_ms;
    }
    
    /**
     * Update network metadata last activity timestamp to current time.
     * Call this whenever the player sends a packet.
     * @param metadata NetworkMetadataComponent to update
     */
    static void update_metadata_activity(NetworkMetadataComponent& metadata) {
        metadata.last_activity = std::chrono::steady_clock::now();
    }
    
    // ========================================================================
    // NetworkEntityComponent Utilities
    // ========================================================================
    
    /**
     * Check if entity position has changed significantly since last sync.
     * @param net_comp NetworkEntityComponent to check
     * @param current_position Current entity position
     * @return true if position change is significant
     */
    static bool has_network_position_changed(const NetworkEntityComponent& net_comp,
                                              const Vec2& current_position) {
        float dx = current_position.x - net_comp.last_synced_position.x;
        float dy = current_position.y - net_comp.last_synced_position.y;
        float dist_sq = dx * dx + dy * dy;
        return dist_sq >= (NetworkEntityComponent::MIN_POSITION_CHANGE * 
                          NetworkEntityComponent::MIN_POSITION_CHANGE);
    }
    
    /**
     * Check if entity state has changed since last sync.
     * @param net_comp NetworkEntityComponent to check
     * @param current_state Current entity state
     * @return true if state has changed
     */
    static bool has_network_state_changed(const NetworkEntityComponent& net_comp,
                                          EntityState current_state) {
        return current_state != net_comp.last_synced_state;
    }
    
    /**
     * Check if any stats have changed since last sync.
     * Directly compares current stats against cached version.
     * @param net_comp NetworkEntityComponent to check
     * @param current_stats Current stats component
     * @return true if stats have changed
     */
    static bool has_network_stats_changed(const NetworkEntityComponent& net_comp,
                                          const Stats& current_stats) {
        if (!net_comp.last_synced_stats) {
            return true;  // No cached stats, so it's a change
        }
        // Simple field-by-field comparison of critical fields
        return current_stats.health != net_comp.last_synced_stats->health ||
               current_stats.max_health != net_comp.last_synced_stats->max_health ||
               current_stats.mana != net_comp.last_synced_stats->mana ||
               current_stats.max_mana != net_comp.last_synced_stats->max_mana ||
               current_stats.level != net_comp.last_synced_stats->level;
    }
    
    /**
     * Mark entity as synced with the given state (updates component data).
     * @param net_comp NetworkEntityComponent to update
     * @param position Current position
     * @param state Current state
     * @param stats Current stats (for change detection)
     * @param current_frame Frame number
     */
    static void mark_network_entity_synced(NetworkEntityComponent& net_comp,
                                           const Vec2& position,
                                           EntityState state,
                                           const Stats* stats,
                                           uint32_t current_frame) {
        net_comp.last_synced_position = position;
        net_comp.last_synced_state = state;
        if (stats) {
            net_comp.last_synced_stats = *stats;
        }
        net_comp.last_sync_frame = current_frame;
        net_comp.force_full_sync_next_frame = false;
    }
    
    // ========================================================================
    // TargetComponent Utilities
    // ========================================================================
    
    /**
     * Set a specific target on the target component.
     * @param target_comp TargetComponent to update
     * @param target_id Entity ID to target
     */
    static void set_target(TargetComponent& target_comp, EntityID target_id) {
        target_comp.current_target = target_id;
        target_comp.has_target = (target_id != INVALID_ENTITY_ID);
        target_comp.last_target_search_ms = 0.0f;
    }
    
    /**
     * Clear current target from the target component.
     * @param target_comp TargetComponent to update
     */
    static void clear_target(TargetComponent& target_comp) {
        target_comp.current_target = INVALID_ENTITY_ID;
        target_comp.has_target = false;
        target_comp.target_in_range = false;
        target_comp.distance_to_target = 0.0f;
    }
    
    /**
     * Check if should search for new target.
     * @param target_comp TargetComponent to check
     * @param current_time_ms Current game time
     * @return true if enough time has passed since last search
     */
    static bool should_search_for_target(const TargetComponent& target_comp,
                                         float current_time_ms) {
        return (current_time_ms - target_comp.last_target_search_ms) >= 
               target_comp.target_search_interval_ms;
    }
    
    /**
     * Mark that target search was performed.
     * @param target_comp TargetComponent to update
     * @param current_time_ms Current game time
     */
    static void mark_target_searched(TargetComponent& target_comp, float current_time_ms) {
        target_comp.last_target_search_ms = current_time_ms;
    }
    
    /**
     * Check if current target is still valid.
     * @param target_comp TargetComponent to check
     * @return true if target exists and is in range
     */
    static bool is_target_valid(const TargetComponent& target_comp) {
        return target_comp.has_target && target_comp.current_target != INVALID_ENTITY_ID && 
               target_comp.distance_to_target <= target_comp.target_loss_range;
    }
    
    /**
     * Update distance to target (called by systems when calculating distance).
     * @param target_comp TargetComponent to update
     * @param distance Current distance in game units
     * @param attack_range Attack range threshold
     */
    static void update_target_distance(TargetComponent& target_comp,
                                       float distance,
                                       float attack_range) {
        target_comp.distance_to_target = distance;
        target_comp.target_in_range = (distance <= attack_range);
    }
};
