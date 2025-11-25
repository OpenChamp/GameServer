#pragma once

#include <systems/entity_manager.hpp>
#include <systems/system_context.hpp>
#include <systems/util/combat_system.hpp>
#include <components/attack.hpp>

/**
 * AttackExecutionSystem - Executes pending attacks and applies damage
 * 
 * RESPONSIBILITIES:
 *   - Update attack animation progress each frame
 *   - Detect when damage should be applied based on animation timing
 *   - Call CombatSystem.apply_damage() at the right moment
 *   - Clean up completed attacks
 *   - Reset attack state after animations complete
 * 
 * COMPONENTS USED:
 *   - AttackComponent (attack state and pending attack data)
 *   - AutoAttackComponent (animation timing parameters)
 *   - Stats (for damage calculation)
 * 
 * SYSTEM INTERACTION:
 *   - Runs AFTER AutoAttackSystem (which queues attacks)
 *   - Runs AFTER CombatSystem (separate pass for execution)
 *   - Calls CombatSystem.apply_damage() to process damage
 * 
 * DATA-DRIVEN DESIGN:
 *   Attacks are queued with pending data (damage, target, animation duration).
 *   This system executes those attacks based on the configured animation timing.
 */
class AttackExecutionSystem {
public:
    /**
     * Set reference to combat system for applying damage.
     * Must be called before update().
     * @param combat_system Reference to CombatSystem
     */
    void set_combat_system(CombatSystem* combat_system) {
        combat_system_ = combat_system;
    }

    /**
     * Update all pending attacks for this frame.
     * Processes attack animations and applies damage at the correct time.
     * @param ctx System context with entity manager, services, and delta time
     */
    void update(const SystemContext& ctx);

private:
    CombatSystem* combat_system_ = nullptr;

    /**
     * Process a single entity's pending attack.
     * @param ctx System context
     * @param entity Entity with attack in progress
     * @param attack Attack component with pending data
     * @param delta_time_ms Time elapsed this frame
     * @return true if damage was applied this frame
     */
    bool execute_attack(const SystemContext& ctx, Entity& entity, AttackComponent& attack, float delta_time_ms);

    /**
     * Check if it's time to apply damage based on animation progress and hit timing.
     * @param attack_progress Current animation progress (0.0-1.0)
     * @param hit_timing_percent When damage should occur (0.0-1.0)
     * @return true if we just crossed the hit timing threshold
     */
    bool is_damage_apply_time(float attack_progress, float hit_timing_percent) const;
};
