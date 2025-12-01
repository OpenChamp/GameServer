#pragma once

#include "system_context.hpp"

/**
 * NPC System - handles NPC entity behavior.
 * 
 * Handles all entities with NPCComponents. Analyses their
 * type and determines what they should do next.
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
    
    void update(const SystemContext& ctx);

private:
    void handle_minion_update(const SystemContext& ctx, Entity* entity);
};