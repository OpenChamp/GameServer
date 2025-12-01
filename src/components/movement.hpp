#pragma once

#include <components/component.hpp>
#include <libs/math.hpp>
#include <vector>
#include <optional>

/**
 * Movement component for entities.
 * Handles position, velocity, and collision (physicality) -- cmkrist 15/11/2025
 */
struct Movement : public Component {
    // Position (Rotation is client-side for now)
    Vec2 position{0.0f, 0.0f};
    Vec2 velocity{0.0f, 0.0f};
    
    // Collision radius for entity-to-entity collision avoidance
    float collision_radius{0.5f}; 

    // Position that the entity currently wants to move to
    std::optional<Vec2> target = std::nullopt;
    
    COMPONENT_TYPE_ID(Movement, 2001)
};