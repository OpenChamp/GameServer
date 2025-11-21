#pragma once

#include "system_context.hpp"
/**
 * NPC System - Handles entities that need to act on the map
 * but do not have a player controlling them.
 * 
 * RESPONSIBILITIES:
 *   - Make NPC entities do things
 * 
 * FEATURES:
 *   - Checks what the entity is thinking
 *   - Checks what the entity is doing
 *   - If the entitiy isn't doing what the system thinks it
 *      should be doing, makes the entity do that thing
 * 
 * USAGE:
 *   NPCSystem npc_system;
 *   // In game loop:
 *   npc_system.update(ctx);
 * 
 * SYSTEM INTERACTION:
 *   - Runs before all other systems (except input) because this is kind of 
 *      like "npc input"
 *   - Can be used independently or as part of GameplayCoordinator
 */
class NPCSystem {
public:
    NPCSystem() = default;
    ~NPCSystem() = default;
    
    // Prevent copying
    NPCSystem(const NPCSystem&) = delete;
    NPCSystem& operator=(const NPCSystem&) = delete;
    
    // Allow moving
    NPCSystem(NPCSystem&&) = default;
    NPCSystem& operator=(NPCSystem&&) = default;
    
    /**
     * Update npc entity actions.
     * 
     * @param ctx System context with entity manager and network service
     */
    void update(const SystemContext& ctx);

private:
    /**
     * Update a single entity.
     * This allows us to return at any point the entity is done thinking,
     * which we could not do inside a loop in the main update function.
     * 
     * @param System System context with entity manager and network service
     * @param npc_entity The entity that is currently thinking
     */
    void update_entity(const SystemContext& ctx, Entity* npc_entity);
};