#pragma once

#include "entity_manager.hpp"
#include "system_context.hpp"
#include "components/stats.hpp"
#include <vector>

/**
 * CombatSystem - Handles damage calculation and application
 * 
 * RESPONSIBILITIES:
 *   - Damage calculation and application
 *   - Critical hit calculation
 *   - Armor/resist/mitigation calculations
 *   - Lifesteal and spell vamp application
 *   - Entity death state management
 * 
 * DATA-DRIVEN DESIGN:
 *   This system is purely data-driven. It only applies damage, healing, and
 *   mana restoration based on external input (from skills, auto-attacks, etc).
 *   Auto-attack behavior, cooldown management, and target selection are handled
 *   by the AutoAttackSystem.
 * 
 * COMPONENTS USED:
 *   - Stats (base damage, armor, resist, health)
 *   - EntityStateComponent (entity state tracking)
 *   - NetworkEntityComponent (for syncing death state)
 * 
 * SYSTEM INTERACTION:
 *   - Works with NetworkSyncSystem to broadcast combat effects
 *   - AutoAttackSystem calls apply_damage to execute attacks
 *   - Triggered by other systems for damage/healing events
 *   - Coordinates with GameplayCoordinator for execution order
 */
class CombatSystem {
public:
    /**
     * Update all combat for this frame.
     * The combat system is data-driven - actual combat is triggered externally.
     * @param ctx System context with entity manager, services, and delta time
     */
    
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
