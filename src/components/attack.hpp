#pragma once

#include "component.hpp"
#include <cstdint>
#include <optional>

/**
 * AttackComponent - Tracks current attack state and cooldown (DATA ONLY)
 * 
 * USAGE: Add to any entity that can attack (champions, minions, towers) and projectiles
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
    // The entity that this component currently wants to target
    std::optional<EntityID> target;

    // === Attack State ===
    bool can_attack = true;                         // Can attack this frame
    
    // === Cooldown Tracking ===
    const float attack_cooldown_ms = 1000.0f;      // Amount of ms to wait after attacking before being able to attack again (set once)
    float cooldown_timer_ms = 0.0f;                // Tracks remaining cooldown time
    float cast_time_ms = 0.0f;                     // Time spent in current attack animation
    float last_attack_time_ms = 0.0f;              // Timestamp of last attack
    float hit_timing_percent = 0.5f;                // When in animation does damage occur (0.0-1.0)
    
    // === Current Attack ===
    bool attack_in_progress = false;               // Currently executing attack animation/projectile
    float attack_animation_progress = 0.0f;        // 0.0 to 1.0, used for timing hit/effects
    float attack_animation_duration_ms = 300.0f;   // Total duration of attack animation
    
    // === Pending Attack Data (set by CombatSystem) ===
    float pending_damage = 0.0f;                   // Damage to apply when animation completes
    EntityID pending_target = INVALID_ENTITY_ID;   // Target of pending attack
    DamageType pending_damage_type = DamageType::PHYSICAL;  // Type of pending damage
    
    // === Attack Counters ===
    uint32_t total_attacks = 0;                    // Total attacks landed (for stats/quests)
    uint32_t attacks_this_frame = 0;               // Prevent multiple attacks per frame
    
    COMPONENT_TYPE_ID(AttackComponent, 2020)
};

