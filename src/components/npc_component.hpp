#pragma once

#include "component.hpp"
#include "component_registry.hpp"

enum class NPCType {
    MINION
};

struct NPCComponent : public Component {
    NPCType npc_type;
    float chase_distance = 30.0f;
    float chase_timeout_ms = 5000.0f; // How long to keep chasing without hitting the target (not implemented)-- cmkrist 24/11/2025
    
    // === State Cooldown ===
    float state_change_cooldown_ms = 250.0f;  // Minimum time between state changes (prevents rapid flip-flopping)
    float time_since_last_state_change_ms = 0.0f;  // Tracks time since last state change
    
    /**
     * Check if enough time has passed to allow a state change.
     * Prevents rapid state transitions that cause minions to get stuck.
     * @return true if state change is allowed
     */
    bool can_change_state() const {
        return time_since_last_state_change_ms >= state_change_cooldown_ms;
    }
    
    /**
     * Mark that a state change just occurred.
     * Resets the cooldown timer.
     */
    void on_state_changed() {
        time_since_last_state_change_ms = 0.0f;
    }
    
    /**
     * Update the state change cooldown timer.
     * @param delta_time_ms Time elapsed since last update
     */
    void update_state_cooldown(float delta_time_ms) {
        if (time_since_last_state_change_ms < state_change_cooldown_ms) {
            time_since_last_state_change_ms += delta_time_ms;
        }
    }

    COMPONENT_TYPE_ID(NPCComponent, ComponentTypes::NPC)
};