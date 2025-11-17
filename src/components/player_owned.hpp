#pragma once

#include "component.hpp"
#include <cstdint>

/**
 * PlayerOwnedComponent - Links a champion/entity to its owning player
 * 
 * USAGE: Add to champion entities to track which player controls them
 * SYSTEMS: PlayerManager, InputSystem
 * 
 * PURPOSE:
 *   - Link champion entities to their player entity
 *   - Allow player inputs to control champion behavior
 *   - Track ownership for damage/kill credit
 * 
 * BENEFITS:
 *   - Decouples player (input/connection) from champion (in-game avatar)
 *   - Multiple champions per player possible (future feature)
 *   - Clean architecture like League of Legends
 * 
 * EXAMPLE:
 *   // Find the champion controlled by a player
 *   auto all_champions = entity_manager.get_entities_with_component<PlayerOwnedComponent>();
 *   for (auto* champion : all_champions) {
 *       if (champion->get_component<PlayerOwnedComponent>()->owning_player_id == player_id) {
 *           // This is the player's champion
 *       }
 *   }
 */
struct PlayerOwnedComponent : public Component {
    EntityID owning_player_id = INVALID_ENTITY_ID;  // Entity ID of the owning player
    
    COMPONENT_TYPE_ID(PlayerOwnedComponent, 3005)
};
