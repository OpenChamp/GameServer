#pragma once

#include "system_context.hpp"
#include <cstdint>

/**
 * CollisionSystem - Handles entity collision detection and response
 * 
 * RESPONSIBILITIES:
 *   - Detect collisions between moving entities
 *   - Push entities out of collision when they intersect
 *   - Report collision events
 *   - Prevent entities from overlapping
 * 
 * FEATURES:
 *   - Circle-based collision detection (using collision_radius)
 *   - Elastic collision response (pushing entities apart)
 *   - Works with Movement component's collision_radius
 * 
 * USAGE:
 *   CollisionSystem collision_system;
 *   // In game loop:
 *   collision_system.update(ctx);
 * 
 * SYSTEM INTERACTION:
 *   - Runs before MovementSystem to prevent overlaps before they happen
 *   - Can be used independently or as part of GameplayCoordinator
 *   - Respects entity movement and collision radius
 */
class CollisionSystem {
public:
    CollisionSystem() = default;
    ~CollisionSystem() = default;
    
    // Prevent copying
    CollisionSystem(const CollisionSystem&) = delete;
    CollisionSystem& operator=(const CollisionSystem&) = delete;
    
    // Allow moving
    CollisionSystem(CollisionSystem&&) = default;
    CollisionSystem& operator=(CollisionSystem&&) = default;
    
    /**
     * Update collision detection and response for all entities.
     * Detects overlapping entities and pushes them apart.
     * 
     * @param ctx System context containing entity manager
     */
    void update(const SystemContext& ctx);
    
    /**
     * Check if a position would collide with any other entity.
     * Does not perform response, only detection.
     * 
     * @param entity The entity to check collisions for
     * @param proposed_position The position to check
     * @param entity_manager Reference to entity manager
     * @return true if collision would occur at proposed position
     */
    static bool would_collide(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager);
    
    /**
     * Get the first entity that would collide at a position.
     * Useful for checking what specifically is blocking movement.
     * 
     * @param entity The entity to check collisions for
     * @param proposed_position The position to check
     * @param entity_manager Reference to entity manager
     * @return Entity pointer if collision found, nullptr otherwise
     */
    static Entity* get_colliding_entity(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager);
    
    /**
     * Push an entity away from another entity.
     * Moves the pushed entity along the separation direction.
     * 
     * @param entity The entity doing the pushing
     * @param pushed_entity The entity being pushed
     * @param push_direction Direction to push (usually direction of pushing entity's movement)
     * @param push_distance How far to push
     */
    static void push_entity(Entity& pushed_entity, const Vec2& push_direction, float push_distance);
    
    /**
     * Push all entities colliding at a position away from it.
     * When an entity moves into other entities, pushes all of them along the movement direction.
     * 
     * @param entity The entity doing the pushing
     * @param proposed_position The position being moved into
     * @param entity_manager Reference to entity manager for finding colliding entities
     */
    static void push_colliding_entities(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager);
    
    /**
     * Find a free spawn position near a given location.
     * Searches for a collision-free position using expanding circle pattern.
     * 
     * @param position The desired spawn position
     * @param collision_radius The collision radius of the entity to spawn (default 1.0)
     * @param entity_manager Reference to entity manager to check existing entities
     * @return A nearby collision-free position, or the original position if none found
     */
    static Vec2 find_free_space(const Vec2& position, float collision_radius, EntityManager& entity_manager);

private:
    /**
     * Calculate distance between two 2D points.
     * @param a First point
     * @param b Second point
     * @return Distance
     */
    static float distance(const Vec2& a, const Vec2& b);
};
