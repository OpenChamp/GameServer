#pragma once

#include "component.hpp"
#include "component_registry.hpp"

enum class NPCType {
    MINION
};



struct NPCComponent : public Component {
    NPCType npc_type;
    
    enum class AIPersonality {
        PASSIVE,                // No aggression, only retaliate
        DEFENSIVE,              // Attack if threatened, otherwise passive
        BALANCED,               // Normal minion behavior
        AGGRESSIVE,             // Actively seek targets
        ZEALOUS                 // Very aggressive, ignore retreat
    };
    
    // === Behavior Configuration ===
    AIPersonality personality = AIPersonality::BALANCED;
    float aggression_range = 10.0f;                // How far to search for enemies to attack
    float retreat_range = 8.0f;                    // How far to retreat if outnumbered
    bool aggressive = true;                        // Search for targets actively
    bool retaliate_only = false;                   // Only attack if attacked first
    
    // === Target Preference ===
    bool prioritize_enemies = true;                // Attack enemies over neutrals
    bool prioritize_wounded = true;                // Focus low-health targets
    bool prioritize_attackers = false;             // Focus entities attacking me
    bool prioritize_closest = true;                // Among valid targets, choose closest
    
    // === Chase Behavior ===
    float chase_distance = 30.0f;
    float chase_timeout_ms = 5000.0f;              // How long to keep chasing without hitting the target
    
    // === State Cooldown ===
    float state_change_cooldown_ms = 250.0f;       // Minimum time between state changes (prevents rapid flip-flopping)
    float time_since_last_state_change_ms = 0.0f;  // Tracks time since last state change

    COMPONENT_TYPE_ID(NPCComponent, ComponentTypes::NPC)
};