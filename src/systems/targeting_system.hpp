#pragma once

#include "entity_manager.hpp"
#include "components/auto_attack.hpp"
#include "components/movement.hpp"
#include "components/stats.hpp"
#include "systems/math.hpp"
#include <vector>
#include <memory>

/**
 * Targeting and Combat Detection System
 * 
 * Responsible for:
 * - Detecting potential targets within entity vision ranges
 * - Validating target eligibility (team, state, distance)
 * - Managing auto-attack target acquisition and retention
 * - Triggering attacks when targets enter attack range
 * - Coordinating with CombatSystem for actual damage application
 */
class TargetingSystem {
public:
    /**
     * Targeting event for when an entity acquires or loses a target
     */
    struct TargetingEvent {
        EntityID entity_id;        // Entity that acquired/lost target
        EntityID target_id;        // The target entity (0 if lost target)
        bool acquired;             // true if acquired, false if lost
    };

    /**
     * Combat detection event for when an entity enters attack range
     */
    struct CombatDetectionEvent {
        EntityID attacker_id;      // Entity initiating combat
        EntityID target_id;        // Target entity
        float distance;            // Current distance to target
        float attack_range;        // Attack range of attacker
    };

    TargetingSystem() = default;
    virtual ~TargetingSystem() = default;

    /**
     * Update targeting system - detect targets and manage auto-attacks
     * Should be called once per frame/tick from the main game loop
     * @param entity_manager Reference to the entity manager
     * @param delta_time Time elapsed since last update (in seconds)
     * @return Vector of targeting events that occurred during this update
     */
    std::vector<TargetingEvent> update(
        EntityManager& entity_manager,
        float delta_time
    );

    /**
     * Get all targets within an entity's vision range
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity searching for targets
     * @param include_allies Whether to include allies in results (default: false)
     * @return Vector of entity IDs within vision range
     */
    std::vector<EntityID> get_targets_in_range(
        EntityManager& entity_manager,
        EntityID entity_id,
        bool include_allies = false
    ) const;

    /**
     * Check if a target is within attack range of an attacker
     * @param attacker_position Position of the attacking entity
     * @param target_position Position of the target entity
     * @param attack_range Attack range in world units
     * @return true if target is within attack range
     */
    bool is_in_attack_range(
        const Vec3& attacker_position,
        const Vec3& target_position,
        float attack_range
    ) const;

    /**
     * Check if a target is within vision range of an observer
     * @param observer_position Position of the observing entity
     * @param target_position Position of the target entity
     * @param vision_range Vision range in world units
     * @return true if target is within vision range
     */
    bool is_in_vision_range(
        const Vec3& observer_position,
        const Vec3& target_position,
        float vision_range
    ) const;

    /**
     * Acquire a target for an entity
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity acquiring the target
     * @param target_id ID of the target to acquire
     * @return true if target was acquired successfully
     */
    bool acquire_target(
        EntityManager& entity_manager,
        EntityID entity_id,
        EntityID target_id
    );

    /**
     * Release the current target of an entity
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity releasing its target
     * @return true if target was released (entity had a target)
     */
    bool release_target(
        EntityManager& entity_manager,
        EntityID entity_id
    );

    /**
     * Check if two entities are on opposing teams/can attack each other
     * For now, this checks if both have Movement components (minions can attack each other)
     * @param entity_manager Reference to the entity manager
     * @param entity_id Entity to check
     * @param target_id Potential target to check
     * @return true if entities can engage in combat
     */
    bool can_engage_in_combat(
        EntityManager& entity_manager,
        EntityID entity_id,
        EntityID target_id
    ) const;

    /**
     * Check if a target is still valid (alive, in range, can engage)
     * @param entity_manager Reference to the entity manager
     * @param entity_id Entity that has the target
     * @param target_id Current target ID to validate
     * @return true if target is still valid
     */
    bool is_target_valid(
        EntityManager& entity_manager,
        EntityID entity_id,
        EntityID target_id
    ) const;

    /**
     * Get the closest enemy target within vision range
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity searching for targets
     * @return ID of closest enemy, or INVALID_ENTITY_ID if none found
     */
    EntityID get_closest_target(
        EntityManager& entity_manager,
        EntityID entity_id
    ) const;

private:
    /**
     * Calculate distance between two entities
     * @param pos1 First position
     * @param pos2 Second position
     * @return Distance between positions
     */
    float calculate_distance(const Vec3& pos1, const Vec3& pos2) const;

    /**
     * Process auto-attack logic for an entity
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity to process
     * @param delta_time Time elapsed since last update
     * @return TargetingEvent if a targeting change occurred, nullptr otherwise
     */
    std::vector<TargetingEvent> process_auto_attack(
        EntityManager& entity_manager,
        EntityID entity_id,
        float delta_time
    );

    /**
     * Attempt to find a new target for an entity
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity searching for a target
     * @return ID of new target, or INVALID_ENTITY_ID if none found
     */
    EntityID find_new_target(
        EntityManager& entity_manager,
        EntityID entity_id
    );
};
