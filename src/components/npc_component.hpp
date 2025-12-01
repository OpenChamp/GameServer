#pragma once

#include "component.hpp"
#include "component_registry.hpp"

enum class NPCType {
    MINION
};



struct NPCComponent : public Component {
    NPCType npc_type;

    
    // === Behavior Configuration ===
    float aggression_range = 10.0f;                // How far to search for enemies to attack
    Vec2 objective;                                // Target position for minions to path towards

    // === Chase Behavior ===
    float chase_distance = 30.0f;
    float chase_timeout_ms = 5000.0f;              // How long to keep chasing without hitting the target
    
    // === State Cooldown ===
    float state_change_cooldown_ms = 250.0f;       // Minimum time between state changes (prevents rapid flip-flopping)
    float time_since_last_state_change_ms = 0.0f;  // Tracks time since last state change

    COMPONENT_TYPE_ID(NPCComponent, ComponentTypes::NPC)
};