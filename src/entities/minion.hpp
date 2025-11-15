#pragma once

#include "../components/stats.hpp"
#include "../components/state.hpp"
#include "../components/movement.hpp"

/**
 * ONLY HERE FOR VISUAL REPRESENTATION, DO NOT USE DIRECTLY. -- cmkrist 15/11/2025
 */

struct Minion {
    Stats stats;
    State state;
    Movement movement;
    Minion() {
        /* DEFAULT MINION STATS */
        stats.max_health = 100.0f;
        stats.health = 100.0f;
        stats.move_speed = 3.0f;
        stats.physical_power = 5.0f;
        stats.magic_power = 10.0f;
        stats.vision_range = 1.0f;

        /* DEFAULT STATE */
        state.current_state = EntityState::IDLE;

        /* DEFAULT MOVEMENT */
        movement.move_speed = stats.move_speed;
    }
};

struct MeleeMinion : public Minion {
    MeleeMinion() { } // Uses default Minion stats
};

struct CannonMinion : public Minion {
    CannonMinion() {
        stats.max_health = 150.0f;
        stats.health = 150.0f;
        stats.physical_power = 15.0f;
        stats.vision_range = 4.0f;
    }
};

struct RangedMinion : public Minion {
    RangedMinion() {
        stats.max_health = 80.0f;
        stats.health = 80.0f;
        stats.physical_power = 10.0f;
        stats.vision_range = 5.0f;
    }
};

struct MagicMinion : public Minion {
    MagicMinion() {
        stats.max_health = 80.0f;
        stats.health = 80.0f;
        stats.physical_power = 10.0f;
        stats.vision_range = 5.0f;
    }
};