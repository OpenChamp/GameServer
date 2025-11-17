#pragma once

#include "entity_manager.hpp"
#include "components/stats.hpp"
#include "components/movement.hpp"
#include "components/map.hpp"
#include "math.hpp"
#include <vector>

// Forward declaration to avoid circular dependencies
class NavigationService;

/**
 * System to handle entity movement along pathfinding waypoints.
 * Updates positions based on movement speed and follows path waypoints.
 * Automatically requests new paths when reaching target spawnpoints.
 */
class MovementSystem {
public:
    /**
     * Update all moving entities.
     * Moves entities toward their current waypoints and handles path progression.
     * @param entity_manager Reference to the entity manager
     * @param delta_time Time elapsed since last update in seconds
     * @param navigation_service Optional pointer to navigation service for requesting new paths
     * @param map Optional pointer to map for spawnpoint information
     */
    void update(EntityManager& entity_manager, float delta_time, NavigationService* navigation_service = nullptr, const Map* map = nullptr);
    
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
    
    /**
     * Request a new path for an entity to a target spawnpoint.
     * @param entity The entity requesting a path
     * @param target_spawnpoint_id Target spawnpoint index
     * @param navigation_service Navigation service to queue the request
     * @param map Map containing spawnpoint information
     */
    static void request_new_path(Entity& entity, uint32_t target_spawnpoint_id, NavigationService* navigation_service, const Map* map);
    
    /**
     * Check for collision with other entities.
     * @param entity The entity to check collisions for
     * @param proposed_position The position the entity wants to move to
     * @param entity_manager Reference to entity manager for checking other entities
     * @return true if collision detected, false if path is clear
     */
    static bool has_collision(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager);
    
    /**
     * Calculate distance between two points.
     * @param a First point
     * @param b Second point
     * @return Distance
     */
    static float distance(const Vec2& a, const Vec2& b);
};

