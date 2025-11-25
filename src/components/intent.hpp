#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <cstdint>

/**
 * Intent types define the high-level goals/behaviors for NPCs.
 * Intents are the "what" - the EntityState is the "how".
 */
enum class IntentType {
    NONE,                   // No active intent
    MOVE_TO_OBJECTIVE,      // Move toward lane objective (minion primary goal)
    MOVE_TO_POSITION,       // Move to a specific position (one-time)
    MOVE_TO_SPAWNPOINT,     // Move along the path to a spawnpoint
    ATTACK_TARGET,          // Attack a specific entity
    DEFEND_POSITION,        // Defend an area around a position
    CHASE_ENEMY,            // Chase down a fleeing enemy
    RETREAT,                // Fall back to a safe position
    PATROL,                 // Patrol a defined area
};

/**
 * Intent priority levels determine which intent takes precedence when multiple are active.
 * Higher values = higher priority.
 */
enum class IntentPriority {
    LOWEST = 0,
    LOW = 1,
    NORMAL = 2,
    HIGH = 3,
    CRITICAL = 4
};

/**
 * IntentComponent manages the NPC's intentions/goals.
 * Stores the current intent state inline with all parameters.
 * Works in conjunction with EntityStateComponent for state management.
 * 
 * Logic for managing intents (setting, clearing, prioritizing) belongs in systems.
 */
struct IntentComponent : public Component {
    
    // === Current Intent State ===
    IntentType type = IntentType::NONE;
    IntentPriority priority = IntentPriority::NORMAL;
    
    // Time this intent has been active (milliseconds)
    float elapsed_time_ms = 0.0f;
    
    // Maximum duration for this intent (0 = unlimited)
    // If exceeded, intent is automatically cleared
    float max_duration_ms = 0.0f;
    
    // Target data (usage depends on intent type)
    uint32_t target_entity_id = INVALID_ENTITY_ID;  // For ATTACK_TARGET, CHASE_ENEMY
    Vec2 target_position = Vec2(0.0f, 0.0f);        // For MOVE_TO_POSITION, DEFEND_POSITION, MOVE_TO_OBJECTIVE
    uint32_t target_spawnpoint_id = 0;              // For MOVE_TO_SPAWNPOINT, MOVE_TO_OBJECTIVE (current lane objective)
    uint32_t objective_index = 0;                   // For MOVE_TO_OBJECTIVE: which spawnpoint in the lane path (0 = next, 1 = after, etc.)
    
    // Behavior parameters
    float min_range = 0.0f;                         // Minimum distance to maintain from target
    float max_range = 0.0f;                         // Maximum distance before intent is abandoned
    float tolerance = 0.1f;                         // Distance tolerance for reaching waypoints/positions
    
    // Whether this intent can be interrupted by higher-priority intents
    bool is_interruptible = true;
    
    COMPONENT_TYPE_ID(IntentComponent, 2011)
};
