#pragma once

#include "component.hpp"
#include "../systems/entity_manager.hpp"
#include <cstdint>

/**
 * TargetComponent - Tracks current attack target
 * 
 * USAGE: Add to entities that need target selection (minions, champions)
 * SYSTEMS: CombatSystem, AutoAttackSystem, TargetingSystem
 * COMPANIONS: AttackComponent, Movement
 * 
 * PURPOSE:
 *   - Track current attack target
 *   - Store target priority and selection criteria
 *   - Support target switching and loss
 *   - Enable target-relative positioning
 * 
 * DESIGN:
 *   - Separates target tracking from attack mechanics
 *   - Enables flexible targeting AI (priority targeting, defensive targeting, etc.)
 *   - Integrates with AttackComponent for actual attacks
 *   - Works with movement systems for approach/positioning
 * 
 * EXAMPLES:
 *   - Minions auto-attack nearest enemy
 *   - Champions focus target selection
 *   - Towers attack closest enemy in range
 */
struct TargetComponent : public Component {
    // === Target Tracking ===
    EntityID current_target = INVALID_ENTITY_ID;       // Entity currently being attacked
    
    // === Target Selection ===
    enum class TargetPriority {
        NEAREST,            // Closest enemy
        HIGHEST_THREAT,     // Enemy dealing most damage
        LOWEST_HEALTH,      // Enemy with lowest HP
        HIGHEST_DAMAGE,     // Enemy with highest attack power
        PRIORITY_TARGET     // Player-selected or set target
    };
    
    TargetPriority targeting_mode = TargetPriority::NEAREST;
    
    // === Target Range ===
    float target_search_range = 10.0f;               // How far to look for targets (game units)
    float target_loss_range = 12.0f;                 // Range beyond which target is lost
    
    // === Target Timing ===
    float last_target_search_ms = 0.0f;              // When last searched for new target
    float target_search_interval_ms = 500.0f;        // How often to search for targets (500ms)
    
    // === Target Status ===
    bool has_target = false;                         // Whether current_target is valid and in range
    bool target_in_range = false;                    // Whether target is in attack range
    float distance_to_target = 0.0f;                 // Cached distance to current target
    
    /**
     * Set a specific target.
     * @param target_id Entity ID to target
     */
    void set_target(EntityID target_id) {
        current_target = target_id;
        has_target = (target_id != INVALID_ENTITY_ID);
        last_target_search_ms = 0.0f;
    }
    
    /**
     * Clear current target.
     */
    void clear_target() {
        current_target = INVALID_ENTITY_ID;
        has_target = false;
        target_in_range = false;
        distance_to_target = 0.0f;
    }
    
    /**
     * Check if should search for new target.
     * @param current_time_ms Current game time
     * @return true if enough time has passed since last search
     */
    bool should_search_for_target(float current_time_ms) const {
        return (current_time_ms - last_target_search_ms) >= target_search_interval_ms;
    }
    
    /**
     * Mark that target search was performed.
     * @param current_time_ms Current game time
     */
    void mark_target_searched(float current_time_ms) {
        last_target_search_ms = current_time_ms;
    }
    
    /**
     * Check if current target is still valid.
     * @return true if target exists and is in range
     */
    bool is_target_valid() const {
        return has_target && current_target != INVALID_ENTITY_ID && 
               distance_to_target <= target_loss_range;
    }
    
    /**
     * Update distance to target (usually called by CombatSystem).
     * @param distance Current distance in game units
     * @param attack_range Attack range threshold
     */
    void update_target_distance(float distance, float attack_range) {
        distance_to_target = distance;
        target_in_range = (distance <= attack_range);
    }
    
    COMPONENT_TYPE_ID(TargetComponent, 2021)
};
