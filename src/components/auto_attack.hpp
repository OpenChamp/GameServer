#pragma once

#include "component.hpp"
#include "stats.hpp"
#include <cstdint>

/**
 * AutoAttackComponent - Pure data for auto-attack mechanics
 * 
 * USAGE: Add to entities that can perform auto-attacks
 * SYSTEMS: CombatSystem
 * COMPANIONS: AttackComponent, TargetComponent, Stats, Movement
 * 
 * PURPOSE:
 *   - Store auto-attack configuration and state
 *   - Track attack cooldown and animation state
 *   - Define attack damage and timing parameters
 *   - Store current combat state
 * 
 * DESIGN:
 *   - Pure data component (no functions)
 *   - CombatSystem reads and updates this data
 *   - BrainSystem controls when auto-attacking is enabled
 *   - No behavior logic - only mechanics
 * 
 * NOTE: Behavior like aggression, target search, and AI personality
 *       belong in NPCComponent and are handled by BrainSystem
 */
struct AutoAttackComponent : public Component {
    // === Auto-Attack State ===
    bool enabled = true;                            // Can auto-attack
    bool in_combat = false;                         // Currently engaged in combat
    float combat_timeout_ms = 5000.0f;              // How long before leaving combat (5 seconds)
    float time_since_attack_ms = 0.0f;              // Time since last attack landed
    
    // === Auto-Attack Damage ===
    float base_damage_multiplier = 1.0f;            // Damage multiplier from base stats
    float bonus_damage = 0.0f;                      // Additional flat damage
    DamageType damage_type = DamageType::PHYSICAL;  // Damage type for resistances
    
    // === Attack Animation ===
    float attack_animation_duration_ms = 300.0f;    // How long attack animation takes
    float hit_timing_percent = 0.5f;                // When in animation does damage occur (0.0-1.0)
    
    // === Kiting & Movement ===
    bool allow_kiting = true;                       // Can move while attacking
    bool kite_while_attacking = false;              // Actively move during auto-attacks
    float movement_during_attack = 0.3f;            // Movement speed multiplier during attack (0.0-1.0)
    
    COMPONENT_TYPE_ID(AutoAttackComponent, 2022)
};
