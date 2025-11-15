#pragma once

#include "../components/stats.hpp"
#include "../components/state.hpp"
#include "../components/movement.hpp"

/**
 * Minion entity.
 * Represents a controllable unit in the game.
 * Components are stats, state, and movement.
 */
struct Minion {
    Stats stats;
    State state;
    Movement movement;
};
