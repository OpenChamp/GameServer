#pragma once

#include "component.hpp"
#include <cstdint>

/**
 * AttackComponent - Tracks current attack state and cooldown (DATA ONLY)
 * 
 * USAGE: Add to any entity that can attack (champions, minions, towers)
 * SYSTEMS: CombatSystem (for all update logic)
 * COMPANIONS: Stats (for attack_speed, attack_range), TargetComponent
 * 
 * PURPOSE:
 *   - Store attack state and cooldown
 *   - Cache attack parameters
 * 
 * DESIGN:
 *   - Pure data component (no methods)
 *   - CombatSystem handles all update logic
 *   - Separates attack state from combat stats
 */
struct AttackComponent : public Component {
    // === Attack State ===
    bool can_attack = true;                         // Can attack this frame
    
    // === Cooldown Tracking ===
    float attack_cooldown_ms = 0.0f;               // Time until next attack available (0 = ready)
    float last_attack_time_ms = 0.0f;              // Timestamp of last attack
    
    // === Current Attack ===
    bool attack_in_progress = false;               // Currently executing attack animation/projectile
    float attack_animation_progress = 0.0f;        // 0.0 to 1.0, used for timing hit/effects
    
    // === Attack Counters ===
    uint32_t total_attacks = 0;                    // Total attacks landed (for stats/quests)
    uint32_t attacks_this_frame = 0;               // Prevent multiple attacks per frame
    
    COMPONENT_TYPE_ID(AttackComponent, 2020)
};

