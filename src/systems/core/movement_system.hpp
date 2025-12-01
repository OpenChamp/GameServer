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

// Determines how far the end point of the current path may be
// off the current movement target before recalculating the path
#define REPATH_DISTANCE 1.0f

/**
 * System to handle entity movement.
 * Updates positions based on movement speed and follows path waypoints
 * for pathfinding units.
 */
class MovementSystem {
public:
    /**
     * Update all moving entities.
     * Moves pathfinding entities toward their current waypoints and handles path progression.
     * Moves entites without pathfinding directly towards their target positions
     * @param ctx System context containing entity manager, navigation service, and map
     */
    void update(const SystemContext& ctx);
    
private:
    // ========================================================================
    // Phase processors
    // ========================================================================
    
    /**
     * Process completed pathfinding results from NavigationService.
     */
    static void process_completed_paths(const SystemContext& ctx);
    
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
    static void update_direct_movement(const SystemContext& ctx, Entity& entity, Movement& movement, Vec2 target, Stats& stats, EntityStateComponent& state_comp);
    
    // ========================================================================
    // Utility functions
    // ========================================================================
    
    /**
     * Request a new path for an entity to a target position.
     * @param entity The entity requesting a path
     * @param target Target position
     * @param navigation_service Navigation service to queue the request
     */
    static void request_new_path(Entity& entity, Vec2 target, NavigationService* navigation_service);
    
    /**
     * Check for collision with other entities.
     * @param entity The entity to check collisions for
     * @param proposed_position The position the entity wants to move to
     * @param entity_manager Reference to entity manager for checking other entities
     * @return true if collision detected, false if path is clear
     */
    static bool has_collision(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager);
};

