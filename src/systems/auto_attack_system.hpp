#pragma once

#include "entity_manager.hpp"
#include "system_context.hpp"
#include "components/auto_attack.hpp"
#include "components/attack.hpp"
#include "components/target.hpp"
#include "components/stats.hpp"

/**
 * AutoAttackSystem - Handles auto-attack AI and cooldown mechanics
 * 
 * RESPONSIBILITIES:
 *   - Automatic target acquisition and searching
 *   - Attack cooldown and readiness tracking
 *   - Combat timeout management
 *   - Target validation and prioritization
 *   - Attack animation management
 *   - Auto-attack execution coordination
 * 
 * COMPONENTS USED:
 *   - AutoAttackComponent (AI behavior settings)
 *   - AttackComponent (attack state and cooldown)
 *   - TargetComponent (target tracking)
 *   - Stats (attack speed, health, damage stats)
 *   - Movement (position for range calculations)
 *   - EntityStateComponent (for state tracking)
 * 
 * SYSTEM INTERACTION:
 *   - Works with CombatSystem which applies actual damage
 *   - Manages when and how attacks are initiated
 *   - Feeds attack information to network sync
 * 
 * DATA-DRIVEN DESIGN:
 *   All auto-attack behavior is configured through AutoAttackComponent properties.
 *   This system reads those properties and executes accordingly without hardcoded logic.
 */
class AutoAttackSystem {
public:
    /**
     * Update all auto-attack behavior for this frame.
     * Handles target search, cooldowns, and attack execution for all entities with AutoAttackComponent.
     * @param ctx System context with entity manager, services, and delta time
     */
    void update(const SystemContext& ctx);

private:
    /**
     * Update and process auto-attacks for an entity.
     * Handles cooldowns, target selection, and attack execution.
     * @param ctx System context
     * @param entity Entity to update
     * @param delta_time_ms Time since last frame in milliseconds
     */
    void update_auto_attack(const SystemContext& ctx, Entity& entity, float delta_time_ms);
    
    /**
     * Find best target for an entity based on targeting mode and auto-attack settings.
     * @param ctx System context
     * @param attacker Entity looking for target
     * @param auto_attack_comp AutoAttackComponent for settings
     * @param target_comp TargetComponent to update
     * @return EntityID of best target, or INVALID_ENTITY_ID if none found
     */
    EntityID find_best_target(const SystemContext& ctx, const Entity& attacker,
                             const AutoAttackComponent& auto_attack_comp,
                             TargetComponent& target_comp) const;
    
    /**
     * Check if a potential target is valid (not dead, correct team, etc).
     * @param ctx System context
     * @param attacker Entity doing attacking
     * @param potential_target Entity to validate
     * @return true if valid target
     */
    bool is_valid_target(const SystemContext& ctx, const Entity& attacker,
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
     * Update attack animation progress.
     * @param attack Attack component to update
     * @param delta_time_ms Time elapsed
     * @param animation_duration_ms Total animation duration
     * @return true if animation is complete
     */
    bool update_attack_animation(AttackComponent& attack, float delta_time_ms, float animation_duration_ms) const;
};
