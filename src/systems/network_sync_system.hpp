#pragma once

#include "entity_manager.hpp"
#include "services/network_service.hpp"

/**
 * System responsible for synchronizing entity state to connected clients.
 * Handles broadcasting entity positions and state changes to maintain
 * consistent game state across all clients.
 */
class NetworkSyncSystem {
public:
    /**
     * Update and broadcast entity state to all clients.
     * @param entity_manager Reference to entity manager for querying entities
     * @param network_service Pointer to network service for broadcasting
     */
    void update(EntityManager& entity_manager, NetworkService* network_service);
};
