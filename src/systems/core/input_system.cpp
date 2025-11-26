#include <systems/core/input_system.hpp>
#include <systems/entity_manager.hpp>
#include <components/entity_state.hpp>
#include <components/movement.hpp>
#include <components/pathfinding.hpp>
#include <services/navigation_service.hpp>
#include <libs/log.hpp>

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
        
        if (process_movement_input(*entity, input.target_position, ctx)) {
            LOG_DEBUG("Successfully processed movement input for entity %u", input.entity_id);
        } else {
            LOG_WARN("Failed to process movement input for entity %u", input.entity_id);
        }
        
        movement_inputs_.pop();
    }
}

bool InputSystem::process_movement_input(Entity& entity, const Vec2& target_position, const SystemContext& ctx) {
    EntityStateComponent* ent_state = entity.get_component<EntityStateComponent>();
    Movement* movement = entity.get_component<Movement>();
    
    if (!ent_state) {
        LOG_WARN("Entity %u missing EntityStateComponent", entity.get_id());
        return false;
    }
    
    if (!movement) {
        LOG_WARN("Entity %u missing Movement component", entity.get_id());
        return false;
    }
    
    // Transform input coordinates from map space to entity space
    // Input comes in as absolute map coordinates, needs to be adjusted to map-relative coordinates
    Vec2 transformed_position = target_position;
    if (ctx.map) {
        float half_width = ctx.map->size.x / 2.0f;
        float half_height = ctx.map->size.y / 2.0f;
        
        // Calculate map minimum (where the map starts)
        float map_min_x = ctx.map->offset.x - half_width;
        float map_min_y = ctx.map->offset.y - half_height;
        
        // Check bounds first
        float max_x = ctx.map->offset.x + half_width;
        float max_y = ctx.map->offset.y + half_height;
        
        if (target_position.x < map_min_x || target_position.x > max_x ||
            target_position.y < map_min_y || target_position.y > max_y) {
            LOG_WARN("Entity %u movement target (%.2f, %.2f) is out of bounds [%.1f-%.1f, %.1f-%.1f]",
                     entity.get_id(), target_position.x, target_position.y,
                     map_min_x, max_x, map_min_y, max_y);
            return false;
        }
        
        // Transform to centered coordinates (offset from map center)
        transformed_position = target_position - ctx.map->offset;
    }
    
    // Attempt to use pathfinding if NavigationService and Map are available
    if (ctx.navigation_service && ctx.map) {
        // Ensure entity has PathfindingComponent
        PathfindingComponent* pathfinding = entity.get_component<PathfindingComponent>();
        if (!pathfinding) {
            auto pf = std::make_unique<PathfindingComponent>();
            entity.add_component(std::move(pf));
            pathfinding = entity.get_component<PathfindingComponent>();
        }
        
        PathRequest request;
        request.entity_id = entity.get_id();
        request.current_position = movement->position;
        request.destination = transformed_position;
        request.entity_pathing_radius = 0.5f;
        
        if (ctx.navigation_service->MakeRequest(request)) {
            // Transition to pathfinding state
            ent_state->current_state = EntityState::PATHFINDING_WAITING;
            ent_state->state_duration_ms = 0.0f;
            ent_state->target_positions.clear();
            
            LOG_INFO("Entity %u requested pathfinding to (%.2f, %.2f)", entity.get_id(), transformed_position.x, transformed_position.y);
            return true;
        } else {
            LOG_WARN("Entity %u failed to request pathfinding", entity.get_id());
            // Fall through to direct movement as fallback
        }
    }
    
    // Fallback: Use direct movement if pathfinding unavailable
    LOG_DEBUG("Entity %u using direct movement to (%.2f, %.2f) (pathfinding unavailable)", 
             entity.get_id(), transformed_position.x, transformed_position.y);
    ent_state->current_state = EntityState::MOVING;
    ent_state->target_positions.clear();
    ent_state->target_positions.push_back(transformed_position);
    
    return true;
}
