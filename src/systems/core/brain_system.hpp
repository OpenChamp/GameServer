#pragma once

#include "system_context.hpp"
#include <components/intent.hpp>
#include <components/entity_state.hpp>
#include <components/stats.hpp>

/**
 * BrainSystem - Intent management
 * 
 * RESPONSIBILITIES:
 *   - Read IntentComponent to understand what the entity wants to do
 *   - Translate high-level intents into low-level component state
 *   - Set up CombatComponent and MovementComponent based on intents
 * 
 * ARCHITECTURE:
 *   This system implements the Intent-based architecture:
 *   - InputSystem/NPCSystem = What the entity thinks it should do (ATTACK_TARGET, MOVE_TO_POSITION, etc.)
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
     * Process a single entity's attack intent.
     * 
     * @param ctx System context
     * @param entity Entity with IntentComponent that intends to attack
     */
    void handle_attack_intent(const SystemContext& ctx, Entity* entity);

    /**
     * Process a single entity's move intent.
     * 
     * @param ctx System context
     * @param entity Entity with intends to move
     */
    void handle_move_intent(const SystemContext& ctx, Entity* entity);};
