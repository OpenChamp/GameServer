#pragma once

#include "component.hpp"
#include "../systems/entity_manager.hpp"
#include <cstdint>

/**
 * TargetComponent - Tracks current attack target
 * 
 * USAGE: Add to entities that need target selection (minions, champions)
 * SYSTEMS: CombatSystem
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
    
    COMPONENT_TYPE_ID(TargetComponent, 2021)
};
