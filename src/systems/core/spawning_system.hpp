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
 *   - Find collision-free spawn positions
 *   - Link spawned entities to their controllers (players, waves, etc.)
 * 
 * COMPONENTS USED:
 *   - Movement (position tracking)
 *   - NetworkEntityComponent (for network synchronization)
 *   - PlayerOwnedComponent (for linking champions to players)
 *   - Stats (for entity attributes)
 * 
 * SYSTEM INTERACTION:
 *   - Called by WaveSystem, other spawning sources
 *   - Works with CollisionSystem to find free space
 *   - Integrates with EntityManager for entity creation
 *   - NetworkSyncSystem handles broadcasting spawned entities to clients
 * 
 * NETWORK COMMUNICATION:
 *   SpawningSystem does NOT directly broadcast to clients. Instead:
 *   1. SpawningSystem creates entity and adds NetworkEntityComponent
 *   2. Sets force_full_sync_next_frame = true on NetworkEntityComponent
 *   3. NetworkSyncSystem detects new entity (last_sync_frame == 0)
 *   4. NetworkSyncSystem broadcasts spawn packet to all clients
 * 
 *   This centralizes all client communication through NetworkSyncSystem.
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
     * Automatically finds a collision-free position if the requested position is occupied.
     * The spawned entity will be automatically synchronized to clients by NetworkSyncSystem
     * on the next frame.
     * 
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
};
