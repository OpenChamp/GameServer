#pragma once

#include "system_context.hpp"
#include <components/intent.hpp>
#include <components/entity_state.hpp>
#include <components/stats.hpp>
#include <components/auto_attack.hpp>

/**
 * BrainSystem - NPC decision-making and intent management
 * 
 * RESPONSIBILITIES:
 *   - Read IntentComponent to understand what the NPC wants to do
 *   - Translate high-level intents into low-level component state
 *   - Manage target acquisition based on intents
 *   - Handle cooldowns and combat timeouts
 *   - Set up CombatComponent and MovementComponent based on intents
 * 
 * ARCHITECTURE:
 *   This system implements the Intent-based architecture:
 *   - IntentComponent = What the NPC thinks it should do (ATTACK_TARGET, MOVE_TO_POSITION, etc.)
 *   - BrainSystem = How to translate intent into lower-level components
 *   - CombatSystem = Reads CombatComponent and executes combat logic
 *   - MovementSystem = Reads MovementComponent and executes movement
 * 
 * FLOW:
 *   1. Read IntentComponent.current_intent
 *   2. Based on intent type, update CombatComponent and MovementComponent
 *   3. CombatSystem will read CombatComponent and handle combat
 *   4. MovementSystem will read MovementComponent and handle movement
 *   5. BrainSystem manages cooldowns and decides when to clear/change intents
 * 
 * USAGE:
 *   BrainSystem brain_system;
 *   // In game loop:
 *   brain_system.update(ctx);
 * 
 * SYSTEM INTERACTION:
 *   - Runs early in the loop (like NPC input)
 *   - Reads: IntentComponent, Stats, Movement, EntityStateComponent, CombatComponent
 *   - Writes: CombatComponent, MovementComponent, EntityStateComponent
 *   - Read by: CombatSystem, MovementSystem
 */
class BrainSystem {
public:
    BrainSystem() = default;
    ~BrainSystem() = default;
    
    // Prevent copying
    BrainSystem(const BrainSystem&) = delete;
    BrainSystem& operator=(const BrainSystem&) = delete;
    
    // Allow moving
    BrainSystem(BrainSystem&&) = default;
    BrainSystem& operator=(BrainSystem&&) = default;
    
    /**
     * Update NPC decision-making and intent management.
     * Translates intents into component state for other systems to execute.
     * 
     * @param ctx System context with entity manager and network service
     */
    void update(const SystemContext& ctx);

private:
    /**
     * Process a single entity's intent.
     * 
     * @param ctx System context
     * @param entity Entity with IntentComponent
     */
    void process_entity_intent(const SystemContext& ctx, Entity* entity);

    /**
     * Update minion AI intent (LoL-style minion behavior).
     * Handles target acquisition and movement to objective.
     * 
     * @param ctx System context
     * @param entity Minion entity
     * @param intent IntentComponent to update
     * @param stats Stats component
     * @param entity_state EntityStateComponent to update
     * @param auto_attack AutoAttackComponent with aggression range
     */
    void update_minion_intent(const SystemContext& ctx, Entity* entity, IntentComponent& intent,
                             Stats& stats, EntityStateComponent& entity_state, 
                             AutoAttackComponent& auto_attack);

    /**
     * Handle generic intent for non-minion entities.
     * Ensures entity state reflects current intent.
     * 
     * @param ctx System context
     * @param entity Entity to update
     * @param intent IntentComponent
     * @param entity_state EntityStateComponent to update
     */
    void handle_generic_intent(const SystemContext& ctx, Entity* entity, 
                              IntentComponent& intent, EntityStateComponent& entity_state);

    /**
     * Get the distance from entity to target.
     * 
     * @param attacker Entity doing the attacking
     * @param target_id ID of target entity
     * @param ctx System context
     * @return Distance in units, or -1 if target not found
     */
    float get_distance_to_target(const Entity* attacker, uint32_t target_id, const SystemContext& ctx) const;
};
