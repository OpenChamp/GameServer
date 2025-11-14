#pragma once

#include "entity_manager.hpp"

/**
 * System to initialize and manage the map and its navmesh.
 * Creates a map entity with a NavMesh component and sets up spawnpoints.
 */
class NavMeshSystem {
public:
    /**
     * Initialize the navmesh system and create the map entity.
     * @param entity_manager Reference to the entity manager
     * @return Reference to the created map entity
     */
    static Entity& initialize_map(EntityManager& entity_manager);
    
    /**
     * Get the map entity ID if it exists.
     * @return Map entity ID, or INVALID_ENTITY_ID if not initialized
     */
    static EntityID get_map_entity_id();
    
private:
    static EntityID map_entity_id_;
};
