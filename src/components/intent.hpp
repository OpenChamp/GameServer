#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <cstdint>

/**
 * Intent types define what entities want to do.
 */
enum class IntentType {
    NONE,                   // No active intent
    MOVE_TO_POSITION,       // Move to a specific position (one-time)
    ATTACK_TARGET,          // Attack a specific entity
};

/**
 * IntentComponent indicates what an entity currently wants to do.
 * The BrainSystem encodes this intention into lower level components
 * such as movement or attack.
 * The Intent is controlled by, for example, the InputSystem (player controlled entity)
 * or the NPCSystem (npc entity).
 */
struct IntentComponent : public Component {
    IntentType type = IntentType::NONE;

    // Target data (usage depends on intent type)
    uint32_t target_entity_id = INVALID_ENTITY_ID;  // For ATTACK_TARGET
    Vec2 target_position = Vec2(0.0f, 0.0f);        // For MOVE_TO_POSITION

    COMPONENT_TYPE_ID(IntentComponent, 2011)
};
