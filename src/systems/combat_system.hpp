#pragma once

#include "entity_manager.hpp"
#include "system_context.hpp"
#include "components/stats.hpp"
#include "components/attack.hpp"
#include "components/auto_attack.hpp"
#include "components/target.hpp"
#include <vector>

/**
 * CombatSystem - Handles all combat mechanics
 * 
 * RESPONSIBILITIES:
 *   - Damage calculation and application
 *   - Auto-attack execution and cooldown management
 *   - Target selection and validation
 *   - Combat state management (in-combat, cooldowns, etc.)
 *   - Critical hit calculation
 *   - Armor/resist/mitigation calculations
 * 
 * COMPONENTS USED:
 *   - Stats (base damage, armor, resist, health)
 *   - AttackComponent (attack cooldown, state)
 *   - AutoAttackComponent (AI behavior)
 *   - TargetComponent (target tracking)
 *   - Movement (for distance calculations)
 * 
 * SYSTEM INTERACTION:
 *   - Works with NetworkSyncSystem to broadcast combat effects
 *   - Integrates with MovementSystem for approach distance
 *   - Coordinates with GameplayCoordinator for execution order
 */
class CombatSystem {
public:
    /**
     * Update all combat for this frame.
     * Processes auto-attacks, cooldowns, and combat state for all entities.
     * @param ctx System context with entity manager, services, and delta time
     */
    void update(const SystemContext& ctx);
    
    /**
     * Structure representing damage information for combat events.
     */
    struct DamageEvent {
        EntityID attacker_id;           // Entity ID of the attacker
        EntityID target_id;             // Entity ID of the target
        float damage_dealt;             // Actual damage applied
        float damage_raw;               // Damage before armor/resist reduction
        DamageType damage_type;         // Type of damage (physical, magical, true)
        bool was_critical_hit;          // Whether this was a critical hit
    };

    /**
     * Apply damage from one entity to another.
     * Calculates damage reduction based on armor/magic resist.
     * Applies lifesteal and spell vamp.
     * @param entity_manager Reference to entity manager
     * @param attacker_id Entity ID of the attacker
     * @param target_id Entity ID of the target
     * @param base_damage Base damage before reductions
     * @param damage_type Type of damage to apply
     * @return DamageEvent containing detailed damage information, or nullptr if invalid
     */
    std::vector<DamageEvent> apply_damage(
        EntityManager& entity_manager,
        EntityID attacker_id,
        EntityID target_id,
        float base_damage,
        DamageType damage_type = DamageType::PHYSICAL
    );

    /**
     * Heal an entity.
     * @param entity_manager Reference to entity manager
     * @param target_id Entity ID of the target to heal
     * @param heal_amount Amount of health to restore
     * @return true if healing was applied, false if entity doesn't exist or has no Stats
     */
    bool apply_heal(
        EntityManager& entity_manager,
        EntityID target_id,
        float heal_amount
    );

    /**
     * Restore mana to an entity.
     * @param entity_manager Reference to entity manager
     * @param target_id Entity ID of the target
     * @param mana_amount Amount of mana to restore
     * @return true if mana was restored, false if entity doesn't exist or has no Stats
     */
    bool restore_mana(
        EntityManager& entity_manager,
        EntityID target_id,
        float mana_amount
    );

    /**
     * Check if an entity is dead (health <= 0).
     * @param entity_manager Reference to entity manager
     * @param entity_id Entity ID to check
     * @return true if entity is dead or doesn't exist, false otherwise
     */
    bool is_dead(
        EntityManager& entity_manager,
        EntityID entity_id
    ) const;
    
    /**
     * Update and process auto-attacks for an entity.
     * Handles cooldowns, target selection, and attack execution for entities with AutoAttackComponent.
     * @param ctx System context
     * @param entity Entity to update
     * @param delta_time_ms Time since last frame in milliseconds
     */
    void update_auto_attack(const SystemContext& ctx, Entity& entity, float delta_time_ms);
    
    /**
     * Find best target for an entity based on targeting mode.
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

private:
    /**
     * Calculate damage reduction based on armor or magic resist.
     * Reduction formula: damage * (100 / (100 + resistance))
     * @param base_damage Base damage before reduction
     * @param resistance Armor or magic resist value
     * @return Reduced damage
     */
    float calculate_damage_reduction(float base_damage, int resistance) const;

    /**
     * Calculate if a critical hit occurs and apply crit bonus.
     * @param attacker Stats of the attacker
     * @return Damage multiplier (1.0 for normal, >1.0 for crit)
     */
    float calculate_critical_hit(const Stats& attacker);
};
