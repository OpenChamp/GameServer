#pragma once

#include <string>
#include <map>
#include <vector>
#include <cstdint>
#include "entity_manager.hpp"

/**
 * PlayerManager - Manages all player entities and their lifecycle
 * 
 * RESPONSIBILITIES:
 *   - Map client_id to player entity IDs
 *   - Handle player connection/disconnection
 *   - Query player readiness
 *   - Track active players
 * 
 * DOES NOT:
 *   - Directly manage entity components (use EntityManager)
 *   - Handle network communication (use NetworkService)
 *   - Manage game state (use GameplayCoordinator)
 * 
 * USAGE:
 *   PlayerManager player_mgr;
 *   EntityID player_id = player_mgr.on_client_connect("client123", entity_manager);
 *   player_mgr.set_player_ready("client123", true, entity_manager);
 *   if (player_mgr.are_all_players_ready(entity_manager)) {
 *       // Start game
 *   }
 */
class PlayerManager {
public:
    PlayerManager() = default;
    ~PlayerManager() = default;
    
    // Prevent copying
    PlayerManager(const PlayerManager&) = delete;
    PlayerManager& operator=(const PlayerManager&) = delete;
    
    // Allow moving
    PlayerManager(PlayerManager&&) = default;
    PlayerManager& operator=(PlayerManager&&) = default;
    
    /**
     * Register a new player entity on client connection.
     * Creates a player entity from the "player" template and applies
     * ClientInfoComponent, ReadinessComponent, and NetworkMetadataComponent.
     * @param client_id Network client identifier
     * @param entity_manager The entity manager (passed for component creation)
     * @return Entity ID of the created player entity, or INVALID_ENTITY_ID on failure
     */
    EntityID on_client_connect(const std::string& client_id, EntityManager& entity_manager);
    
    /**
     * Unregister a player entity on client disconnection.
     * Destroys the player entity and removes all player tracking.
     * @param client_id Network client identifier
     * @param entity_manager The entity manager (passed for entity destruction)
     * @return true if player was found and removed, false if unknown client
     */
    bool on_client_disconnect(const std::string& client_id, EntityManager& entity_manager);
    
    /**
     * Mark a player as ready or not ready.
     * Updates the ReadinessComponent for the player.
     * @param client_id Network client identifier
     * @param is_ready New readiness state
     * @param entity_manager Entity manager for updating component
     * @return true if player was found and updated, false if unknown client
     */
    bool set_player_ready(const std::string& client_id, bool is_ready, EntityManager& entity_manager);
    
    /**
     * Check if all connected players are ready.
     * Queries all entities with ReadinessComponent and verifies all are marked ready.
     * @param entity_manager Entity manager for querying components
     * @return true if all players have is_ready = true, false if any are not ready or no players connected
     */
    bool are_all_players_ready(EntityManager& entity_manager) const;
    
    /**
     * Check if lobby is at capacity.
     * @param max_clients Maximum allowed players
     * @return true if player count >= max_clients
     */
    bool is_full(int max_clients) const;
    
    /**
     * Get number of connected players.
     * @return Count of players currently connected
     */
    size_t get_player_count() const;
    
    /**
     * Get player entity ID by client ID.
     * @param client_id Network client identifier
     * @return Entity ID, or INVALID_ENTITY_ID if not found
     */
    EntityID get_player_entity_id(const std::string& client_id) const;
    
    /**
     * Get champion entity ID by player entity ID.
     * @param player_entity_id Player entity ID
     * @return Entity ID, or INVALID_ENTITY_ID if not found
     */
    EntityID get_champion_entity_id(EntityID player_entity_id, EntityManager& entity_manager) const;
    
    /**
     * Get all connected player entity IDs.
     * @return Vector of entity IDs for all connected players
     */
    std::vector<EntityID> get_all_player_entities() const;
    
    /**
     * Update player connection metadata (latency, last activity).
     * Updates the NetworkMetadataComponent for the player.
     * @param client_id Network client identifier
     * @param entity_manager Entity manager for updating component
     * @param latency_ms Measured latency in milliseconds
     */
    void update_player_latency(const std::string& client_id, EntityManager& entity_manager, unsigned int latency_ms);
    
    /**
     * Reset all players to not-ready state.
     * Used for transitioning between game states (e.g., end of game).
     * @param entity_manager Entity manager for resetting components
     */
    void reset_all_players(EntityManager& entity_manager);
    
    /**
     * Clear all player tracking (typically on shutdown).
     * Removes all client_id to entity mappings but does NOT destroy entities.
     * Entities will be destroyed separately via EntityManager.
     */
    void clear();

private:
    // Map from client_id to player entity ID
    std::map<std::string, EntityID> client_to_entity_;
    
    // Track next player ID for assignment
    uint32_t next_player_id_ = 0;
    
    /**
     * Helper: Create player entity from template and apply all player components.
     * @param client_id Network client identifier
     * @param entity_manager Entity manager for entity creation
     * @return Entity ID of created player, or INVALID_ENTITY_ID on failure
     */
    EntityID create_player_entity(const std::string& client_id, EntityManager& entity_manager);
};
