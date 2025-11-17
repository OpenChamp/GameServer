#pragma once

#include "entity_manager.hpp"
#include "system_context.hpp"
#include <string>

/**
 * SpawningSystem - Handles entity spawning and initialization
 * 
 * RESPONSIBILITIES:
 *   - Spawn entities from templates
 *   - Initialize entity components
 *   - Link spawned entities to their controllers (players, waves, etc.)
 *   - Broadcast spawn events to clients
 * 
 * COMPONENTS USED:
 *   - Movement (position tracking)
 *   - NetworkEntityComponent (for broadcasting to clients)
 *   - PlayerOwnedComponent (for linking champions to players)
 *   - Stats (for entity attributes)
 * 
 * SYSTEM INTERACTION:
 *   - Called by WaveSystem, other spawning sources
 *   - Works with NetworkService to broadcast spawns
 *   - Integrates with EntityManager for entity creation
 * 
 * USAGE:
 *   SpawningSystem spawning_system;
 *   EntityID minion_id = spawning_system.spawn_entity_from_template(
 *       ctx, "melee_minion", position, team_id
 *   );
 */
class SpawningSystem {
public:
    /**
     * Spawn an entity from a template at a specific position.
     * @param ctx System context with entity manager and network service
     * @param template_id Template name (e.g., "melee_minion", "champion")
     * @param position Starting position for the entity
     * @param team_id Team assignment for the entity
     * @return EntityID of spawned entity, or INVALID_ENTITY_ID on failure
     */
    EntityID spawn_entity_from_template(const SystemContext& ctx,
                                       const std::string& template_id,
                                       const Vec2& position,
                                       uint8_t team_id);
    
    /**
     * Broadcast entity spawn to all connected clients.
     * @param ctx System context with network service
     * @param entity_id ID of entity that was spawned
     * @param position Spawn position
     * @param team_id Team of the entity
     * @param template_id Template used for spawning
     */
    void broadcast_entity_spawn(const SystemContext& ctx,
                               EntityID entity_id,
                               const Vec2& position,
                               uint8_t team_id,
                               const std::string& template_id) const;
};
