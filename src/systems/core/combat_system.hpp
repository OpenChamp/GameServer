#pragma once

#include <systems/entity_manager.hpp>
#include <systems/system_context.hpp>
#include <systems/util/combat_calculator.hpp>
#include <systems/util/targeting_utility.hpp>
#include <components/attack.hpp>
#include <components/auto_attack.hpp>
#include <components/npc_component.hpp>
#include <components/target.hpp>

/**
 * CombatSystem - Unified combat management system
 * 
 * RESPONSIBILITIES:
 *   - Auto-attack AI and target acquisition
 *   - Attack cooldown and readiness tracking
 *   - Combat timeout management
 *   - Target validation and prioritization
 *   - Attack animation management
 *   - Attack execution and damage application
 *   - Coordinate between auto-attack logic and damage calculation
 * 
 * REPLACES:
 *   - AutoAttackSystem (auto-attack AI and cooldowns)
 *   - AttackExecutionSystem (attack animation and execution)
 *   This unified system handles all aspects of combat from AI to execution
 * 
 * COMPONENTS USED:
 *   - AutoAttackComponent (AI behavior settings)
 *   - AttackComponent (attack state and cooldown)
 *   - TargetComponent (target tracking)
 *   - Stats (attack speed, health, damage stats)
 *   - Movement (position for range calculations)
 *   - EntityStateComponent (for state tracking)
 * 
 * UTILITIES USED:
 *   - TargetingUtility: Pure utility functions for targeting calculations and validation
 * 
 * SYSTEM INTERACTION:
 *   - Uses CombatCalculator for damage calculation and application
 *   - Uses TargetingUtility for range and eligibility checks
 *   - Works with NetworkSyncSystem to broadcast combat effects
 *   - Feeds attack information to entity state tracking
 * 
 * DATA-DRIVEN DESIGN:
 *   All combat behavior is configured through component properties.
 *   This system reads those properties and executes accordingly without hardcoded logic.
 *   Targeting behavior is entirely data-driven via AutoAttackComponent and Stats properties.
 */
class CombatSystem {
public:
    /**
     * Update all combat for this frame.
     * Handles auto-attacks, cooldowns, target search, and attack execution.
     * @param ctx System context with entity manager, services, and delta time
     */
    void update(const SystemContext& ctx);

private:
    CombatCalculator combat_calculator_;

    /**
     * Update and process auto-attacks for an entity.
     * Handles cooldowns, target selection, and attack initiation.
     * @param ctx System context
     * @param entity Entity to update
     * @param delta_time_ms Time since last frame in milliseconds
     */
    void update_auto_attack(const SystemContext& ctx, Entity& entity, float delta_time_ms);

    /**
     * Execute a pending attack for an entity.
     * Processes attack animations and applies damage at the correct time.
     * @param ctx System context
     * @param entity Entity with attack in progress
     * @param attack Attack component with pending data
     * @param delta_time_ms Time elapsed this frame
     * @return true if damage was applied this frame
     */
    void execute_attack(const SystemContext& ctx, Entity& entity, AttackComponent& attack, float delta_time_ms);

    /**
     * Check if a potential target is valid (not dead, correct team, etc).
     * NOTE: Use TargetingUtility validation functions instead - all targeting is now in utility!
     * @param ctx System context
     * @param attacker Entity doing attacking
     * @param potential_target Entity to validate
     * @return true if valid target
     * @deprecated Use TargetingUtility::can_engage_in_combat() or is_target_valid() directly
     */
    bool is_valid_target(const SystemContext& /*ctx*/, const Entity& attacker,
                        const Entity& potential_target) const;

    /**
     * Check if an entity is dead (health <= 0).
     * @param entity Entity to check
     * @return true if entity is dead
     */
    bool is_dead(const Entity& entity) const;

    /**
     * Update attack cooldown for an entity.
     * Called each frame to reduce the cooldown timer.
     * @param attack Attack component to update
     * @param delta_time_ms Time elapsed since last frame
     */
    void update_attack_cooldown(AttackComponent& attack, float delta_time_ms) const;

    /**
     * Start attack cooldown based on attack speed stat.
     * @param attack Attack component to update
     * @param attack_speed_stat Attack speed from Stats component (attacks per second)
     */
    void start_attack_cooldown(AttackComponent& attack, float attack_speed_stat) const;

    /**
     * Check if an entity is ready to attack.
     * @param attack Attack component to check
     * @return true if cooldown is finished and entity can attack
     */
    bool is_attack_ready(const AttackComponent& attack) const;

    /**
     * Begin attack animation.
     * @param attack Attack component to update
     */
    void begin_attack_animation(AttackComponent& attack) const;

    /**
     * Check if it's time to apply damage based on animation progress and hit timing.
     * @param attack_progress Current animation progress (0.0-1.0)
     * @param hit_timing_percent When damage should occur (0.0-1.0)
     * @return true if we just crossed the hit timing threshold
     */
    bool is_damage_apply_time(float attack_progress, float hit_timing_percent) const;
};
