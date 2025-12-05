#pragma once

#include "component.hpp"

/**
 * ReadinessComponent - Player readiness state
 * 
 * USAGE: Add to player entities for lobby/game readiness tracking
 * SYSTEMS: PlayerManager, GameplayCoordinator
 * 
 * Tracks whether a player has marked themselves as ready to begin
 * the game. Used for lobby state management and game start validation.
 */
struct ReadinessComponent : public Component {
    bool is_ready = false;      // Player has marked themselves ready
    
    COMPONENT_TYPE_ID(ReadinessComponent, 3002)
};
