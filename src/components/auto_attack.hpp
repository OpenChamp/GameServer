#pragma once

#include "component.hpp"
#include "stats.hpp"
#include <cstdint>

/**
 * AutoAttackComponent - AI behavior for automatic attacking
 * 
 * USAGE: Add to minions and other AI entities that should auto-attack
 * SYSTEMS: CombatSystem, AutoAttackSystem
 * COMPANIONS: AttackComponent, TargetComponent, Stats, Movement
 * 
 * PURPOSE:
 *   - Enable AI entities to automatically search for and attack targets
 *   - Separate auto-attack behavior from manual attack mechanics
 *   - Support different AI personalities and aggression levels
 *   - Control when auto-attacking is enabled/disabled
 * 
 * DESIGN:
 *   - Acts as a behavior modifier on top of AttackComponent
 *   - Stores AI-specific parameters
 *   - Integrates with TargetComponent for target selection
 *   - Works with CombatSystem for actual damage resolution
 * 
 * EXAMPLES:
 *   - Minions: Simple aggro, attack nearest enemy in range
 *   - Neutral units: No aggro, attack only if attacked
 *   - Champions (AI): Behavior-driven, target highest threat
 *   - Towers: Aggro-range based, attack closest in range
 */
struct AutoAttackComponent : public Component {
    // === Auto-Attack Control ===
    bool enabled = true;                            // Can auto-attack
    bool aggressive = true;                         // Search for targets actively
    bool retaliate_only = false;                    // Only attack if attacked first
    
    // === Auto-Attack Parameters ===
    float aggression_range = 10.0f;                // How far to search for enemies to attack
    float retreat_range = 8.0f;                    // How far to retreat if outnumbered
    
    // === Behavior Control ===
    enum class AIPersonality {
        PASSIVE,                // No aggression, only retaliate
        DEFENSIVE,              // Attack if threatened, otherwise passive
        BALANCED,               // Normal minion behavior
        AGGRESSIVE,             // Actively seek targets
        ZEALOUS                 // Very aggressive, ignore retreat
    };
    
    AIPersonality personality = AIPersonality::BALANCED;
    
    // === Target Preference ===
    bool prioritize_enemies = true;                 // Attack enemies over neutrals
    bool prioritize_wounded = true;                 // Focus low-health targets
    bool prioritize_attackers = false;              // Focus entities attacking me
    bool prioritize_closest = true;                 // Among valid targets, choose closest
    
    // === State Management ===
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
    
    /**
     * Enable auto-attacking.
     */
    void enable() {
        enabled = true;
    }
    
    /**
     * Disable auto-attacking.
     */
    void disable() {
        enabled = false;
        in_combat = false;
    }
    
    /**
     * Enter combat mode.
     * Used by CombatSystem when combat starts.
     */
    void enter_combat() {
        in_combat = true;
        time_since_attack_ms = 0.0f;
    }
    
    /**
     * Exit combat mode.
     * Called when combat timeout expires.
     */
    void exit_combat() {
        in_combat = false;
    }
    
    /**
     * Update combat timer and potentially exit combat.
     * @param delta_time_ms Time since last frame
     * @return true if just exited combat
     */
    bool update_combat_timer(float delta_time_ms) {
        if (in_combat) {
            time_since_attack_ms += delta_time_ms;
            if (time_since_attack_ms > combat_timeout_ms) {
                exit_combat();
                return true;  // Just exited combat
            }
        }
        return false;
    }
    
    /**
     * Mark that an attack occurred.
     */
    void on_attack() {
        time_since_attack_ms = 0.0f;
        enter_combat();
    }
    
    /**
     * Check if should be searching for targets.
     * @return true if enabled and either not in combat or just exited combat
     */
    bool should_search_targets() const {
        return enabled && aggressive && !retaliate_only;
    }
    
    /**
     * Check if should auto-attack if target is available.
     * @return true if auto-attacking is active
     */
    bool should_auto_attack() const {
        return enabled && (aggressive || in_combat);
    }
    
    /**
     * Get current movement speed modifier based on state.
     * @return Movement speed multiplier (0.0-1.0)
     */
    float get_movement_speed_modifier() const {
        if (in_combat && kite_while_attacking) {
            return movement_during_attack;
        }
        return 1.0f;
    }
    
    COMPONENT_TYPE_ID(AutoAttackComponent, 2022)
};
