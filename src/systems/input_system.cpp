#include "input_system.hpp"
#include "entity_manager.hpp"
#include "components/entity_state.hpp"
#include <log.hpp>

void InputSystem::queue_movement_input(EntityID entity_id, const Vec2& target_position) {
    movement_inputs_.push({entity_id, target_position});
    LOG_DEBUG("Queued movement input for entity %u to (%.2f, %.2f)", entity_id, target_position.x, target_position.y);
}

void InputSystem::update(const SystemContext& ctx) {
    // Process all queued inputs
    while (!movement_inputs_.empty()) {
        const MovementInput& input = movement_inputs_.front();
        
        Entity* entity = ctx.entity_manager.get_entity(input.entity_id);
        if (!entity) {
            LOG_WARN("Movement input for non-existent entity %u", input.entity_id);
            movement_inputs_.pop();
            continue;
        }
        
        if (process_movement_input(*entity, input.target_position)) {
            LOG_DEBUG("Successfully processed movement input for entity %u", input.entity_id);
        } else {
            LOG_WARN("Failed to process movement input for entity %u", input.entity_id);
        }
        
        movement_inputs_.pop();
    }
}

bool InputSystem::process_movement_input(Entity& entity, const Vec2& target_position) {
    EntityStateComponent* ent_state = entity.get_component<EntityStateComponent>();
    
    if (!ent_state) {
        LOG_WARN("Entity %u missing EntityStateComponent", entity.get_id());
        return false;
    }
    
    // Queue the movement target
    ent_state->current_state = EntityState::MOVING;
    ent_state->target_positions.clear();
    ent_state->target_positions.push_back(target_position);
    
    LOG_INFO("Entity %u queued to move to (%.2f, %.2f)", entity.get_id(), target_position.x, target_position.y);
    
    return true;
}
