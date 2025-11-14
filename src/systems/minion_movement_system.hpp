#pragma once

#include "entity_manager.hpp"
#include "components/movement.hpp"
#include "math.hpp"
#include <vector>

/**
 * System to handle minion movement along the navmesh.
 * Moves minions toward their target spawnpoint at their specified speed.
 */
class MinionMovementSystem {
public:
    /**
     * Update minion movement.
     * Moves all minions toward their target spawnpoint.
     * @param entity_manager Reference to the entity manager
     * @param delta_time Time elapsed since last update in seconds
     * @param map_entity Pointer to the map entity (contains NavMesh)
     */
    void update(EntityManager& entity_manager, float delta_time, const Entity* map_entity);
    
private:
    /**
     * Calculate distance between two 3D points.
     * @param a First point
     * @param b Second point
     * @return Euclidean distance
     */
    static float distance(const Vec3& a, const Vec3& b);
    
    /**
     * Normalize a vector (make it unit length).
     * @param v Vector to normalize
     * @return Normalized vector
     */
    static Vec3 normalize(const Vec3& v);
};
