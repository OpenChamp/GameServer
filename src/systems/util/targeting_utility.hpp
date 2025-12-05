#pragma once

#include <systems/entity_manager.hpp>
#include <components/target.hpp>
#include <libs/math.hpp>
#include <vector>
#include <cstdint>

/**
 * TargetingUtility - Data-driven targeting and vision utility functions
 * 
 * Pure utility functions for targeting calculations and target validation.
 * Used by CombatSystem and other systems that need targeting logic.
 * 
 * RESPONSIBILITIES:
 *   - Range and distance calculations (vision, attack range)
 *   - Target eligibility validation (alive, can engage, in range)
 *   - Target discovery (find targets in range, get closest, etc.)
 *   - Team/faction checks for combat eligibility
 * 
 * DATA-DRIVEN DESIGN:
 *   All behavior is configured through component properties:
 *   - AutoAttackComponent: personality, aggression_range, prioritization
 *   - Stats: vision_range, attack_range, health
 *   - TargetComponent: targeting_mode, current_target
 * 
 * USAGE:
 *   // Find best target within vision range
 *   EntityID target = TargetingUtility::get_closest_target(entity_manager, entity_id);
 *   
 *   // Validate if target is still valid
 *   if (TargetingUtility::is_target_valid(entity_manager, attacker_id, target_id)) {
 *       // Can still attack target
 *   }
 * 
 * NOTE:
 *   These are pure utility functions - they do NOT modify entity state.
 *   Only CombatSystem should call these functions in the main update loop.
 */
class TargetingUtility {
public:
    /**
     * Get all targets within an entity's vision range that can be engaged.
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity searching for targets
     * @param include_allies Whether to include allies in results (default: false)
     * @return Vector of entity IDs within vision range and eligible for combat
     */
    static std::vector<EntityID> get_targets_in_range(
        EntityManager& entity_manager,
        EntityID entity_id,
        bool include_allies = false
    );

    /**
     * Check if a target is within attack range of an attacker (2D).
     * @param attacker_position Position of the attacking entity
     * @param target_position Position of the target entity
     * @param attack_range Attack range in world units
     * @return true if target is within attack range
     */
    static bool is_in_attack_range(
        const Vec2& attacker_position,
        const Vec2& target_position,
        float attack_range
    );

    /**
     * Check if a target is within vision range of an observer (2D).
     * @param observer_position Position of the observing entity
     * @param target_position Position of the target entity
     * @param vision_range Vision range in world units
     * @return true if target is within vision range
     */
    static bool is_in_vision_range(
        const Vec2& observer_position,
        const Vec2& target_position,
        float vision_range
    );

    /**
     * Check if two entities can engage in combat.
     * Validates that both entities are alive and have required components.
     * @param entity_manager Reference to the entity manager
     * @param entity_id Entity that would be attacking
     * @param target_id Entity that would be attacked
     * @return true if entities can engage in combat
     */
    static bool can_engage_in_combat(
        EntityManager& entity_manager,
        EntityID entity_id,
        EntityID target_id
    );

    /**
     * Validate if a target is still valid for an attacker.
     * Checks: alive, in vision range, can engage in combat.
     * @param entity_manager Reference to the entity manager
     * @param entity_id Entity that has the target
     * @param target_id Current target ID to validate
     * @return true if target is still valid and can be attacked
     */
    static bool is_target_valid(
        EntityManager& entity_manager,
        EntityID entity_id,
        EntityID target_id
    );

    /**
     * Get the closest enemy target within vision range.
     * @param entity_manager Reference to the entity manager
     * @param entity_id ID of the entity searching for targets
     * @return ID of closest valid target, or INVALID_ENTITY_ID if none found
     */
    static EntityID get_closest_target(
        EntityManager& entity_manager,
        EntityID entity_id
    );

    /**
     * Calculate distance between two positions (2D).
     * @param pos1 First position
     * @param pos2 Second position
     * @return Distance between positions (Euclidean)
     */
    static float calculate_distance(const Vec2& pos1, const Vec2& pos2);

    /**
     * Calculate distance between two positions (2D).
     * @param pos1 First position
     * @param pos2 Second position
     * @return Distance between positions (Euclidean)
     */
    static float calculate_distance(const Vec2& pos1, const EntityID& entity_id, EntityManager& entity_manager);

    /**
     * Find the best target for an entity based on targeting priority and auto-attack settings.
     * Considers distance, health, threat level, and damage based on targeting mode.
     * 
     * @param entity_manager Reference to the entity manager
     * @param attacker_id ID of the entity looking for a target
     * @param aggression_range Maximum distance to search for targets
     * @param targeting_mode TargetComponent::TargetPriority mode for prioritization
     * @param prioritize_closest If true, prefer closer targets as tiebreaker
     * @return ID of best target, or INVALID_ENTITY_ID if none found
     */
    static EntityID find_best_target(
        EntityManager& entity_manager,
        EntityID attacker_id,
        float aggression_range,
        TargetComponent::TargetPriority targeting_mode,
        bool prioritize_closest = true
    );

private:
    // Private constructor - this is a utility class with only static methods
    TargetingUtility() = delete;
    ~TargetingUtility() = delete;
};
