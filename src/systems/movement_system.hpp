#pragma once

#include "entity_manager.hpp"
#include "components/stats.hpp"
#include "components/movement.hpp"
#include "components/map.hpp"
#include "components/pathfinding.hpp"
#include "components/entity_state.hpp"
#include "math.hpp"
#include "system_context.hpp"
#include <vector>

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
     * @param ctx System context containing entity manager, navigation service, and map
     */
    void update(const SystemContext& ctx);
    
private:
    // ========================================================================
    // Phase processors
    // ========================================================================
    
    /**
     * Process pathfinding-related updates (results and retries).
     */
    void process_pathfinding_phase(const SystemContext& ctx);
    
    /**
     * Process completed pathfinding results from NavigationService.
     */
    static void process_completed_paths(const SystemContext& ctx);
    
    /**
     * Retry pending pathfinding requests for stuck entities.
     */
    static void retry_stuck_entities(const SystemContext& ctx);
    
    // ========================================================================
    // Entity movement handlers
    // ========================================================================
    
    /**
     * Update a single entity's movement based on its type.
     */
    void update_entity_movement(const SystemContext& ctx, Entity& entity);
    
    /**
     * Update direct movement (player movement to target positions).
     */
    static void update_direct_movement(const SystemContext& ctx, Entity& entity, Movement& movement, Stats& stats, EntityStateComponent& state_comp);
    
    /**
     * Update stuck detection and trigger repath if needed.
     */
    void update_stuck_detection(const SystemContext& ctx, Entity& entity, Movement& movement, PathfindingComponent& pathfinding, EntityStateComponent* entity_state);
    
    /**
     * Update waypoint-based movement (minion path following).
     */
    static void update_waypoint_movement(const SystemContext& ctx, Entity& entity, Movement& movement, Stats& stats, PathfindingComponent& pathfinding, EntityStateComponent* entity_state);
    
    /**
     * Request path to next spawnpoint when current waypoints exhausted.
     */
    static void request_path_to_next_spawnpoint(const SystemContext& ctx, Entity& entity, PathfindingComponent& pathfinding, EntityStateComponent* entity_state);
    
    // ========================================================================
    // Utility functions
    // ========================================================================
    
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

