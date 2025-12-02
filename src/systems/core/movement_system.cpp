#include <systems/core/movement_system.hpp>
#include <systems/core/collision_system.hpp>
#include <systems/entity_manager.hpp>
#include <services/navigation_service.hpp>
#include <components/pathfinding.hpp>
#include <components/stats.hpp>
#include <components/entity_state.hpp>
#include <components/intent.hpp>
#include <components/player_owned.hpp>
#include <libs/log.hpp>
#include <cmath>

// ============================================================================
// Constants
// ============================================================================
namespace {
    constexpr float TARGET_THRESHOLD = 0.5f;
    constexpr float WAYPOINT_THRESHOLD = 0.5f;
    constexpr float STUCK_DETECTION_THRESHOLD = 0.01f;
}

// ============================================================================
// Movement System Implementation
// ============================================================================

void MovementSystem::update(const SystemContext& ctx) {
    process_completed_paths(ctx);

    auto moving_entities = ctx.entity_manager.get_entities_with_component<Movement>();
    for (auto* entity : moving_entities) {
        update_entity_movement(ctx, *entity);
    }
}

void MovementSystem::update_entity_movement(const SystemContext& ctx, Entity& entity) {
    Movement* movement = entity.get_component<Movement>();
    PathfindingComponent* pathfinding = entity.get_component<PathfindingComponent>();
    Stats* stats = entity.get_component<Stats>();
    EntityStateComponent* state_comp = entity.get_component<EntityStateComponent>();
    
    if (!movement || !stats) return;

    // No movement -> nothing to do
    if (!movement->target.has_value()) {
        return;
    }

    // Entity has arrived at target -> stop moving
    if((movement->position - movement->target.value()).length() <= TARGET_THRESHOLD) {
        movement->target.reset();
        return;
    }

    // Entities without pathfinding move straight towards the target
    if(!pathfinding) {
        update_direct_movement(ctx, entity, *movement, movement->target.value(), *stats, *state_comp);
        return;
    }

    // Pathfinding unit, so find a path
    
    // Entity has already requested a path and is waiting for a response, so no movement this frame
    if(pathfinding->waiting_for_path) {
        return;
    }

    // No path, or path to incorrect target
    if(pathfinding->waypoints.empty() || (pathfinding->waypoints[pathfinding->waypoints.size() - 1] - movement->target.value()).length() > REPATH_DISTANCE) {
        request_new_path(entity, movement->target.value(), ctx.navigation_service);
        return;  // Path request submitted, will process next frame
    }

    // Entity is on correct path, keep moving
    Vec2 next_stop = pathfinding->waypoints.front();

    if(next_stop.distance_to(movement->position) < WAYPOINT_THRESHOLD) {
        pathfinding->waypoints.erase(pathfinding->waypoints.begin());
    }
    // Update direct movement targets (players, point-based movement)
    update_direct_movement(ctx, entity, *movement, next_stop, *stats, *state_comp);
}

void MovementSystem::update_direct_movement(const SystemContext& ctx, Entity& entity, 
                                           Movement& movement, Vec2 target, Stats& stats, EntityStateComponent& state_comp) {
    Vec2 current_pos = movement.position;
    Vec2 target_pos = target;
    Vec2 direction = target_pos - current_pos;
    float distance = direction.length();
    
    // Calculate new position based on speed
    float move_speed = stats.move_speed;
    float distance_to_move = move_speed * (ctx.delta_time_ms / 1000.0f);
    
    Vec2 new_position = (distance_to_move >= distance) 
        ? target_pos 
        : current_pos + (direction.normalized() * distance_to_move);
    
    movement.position = new_position;
}

// ============================================================================
// Utility Functions
// ============================================================================

bool MovementSystem::has_collision(const Entity& entity, const Vec2& proposed_position, EntityManager& entity_manager) {
    const Movement* entity_move = entity.get_component<Movement>();
    if (!entity_move) {
        return false;
    }
    
    float entity_radius = entity_move->collision_radius;
    
    // Check collision with all other moving entities
    auto moving_entities = entity_manager.get_entities_with_component<Movement>();
    for (auto* other_entity : moving_entities) {
        if (other_entity->get_id() == entity.get_id()) {
            continue;  // Skip self
        }
        
        const Movement* other_move = other_entity->get_component<Movement>();
        if (!other_move) {
            continue;
        }
        
        float other_radius = other_move->collision_radius;
        float min_distance = entity_radius + other_radius;
        
        float actual_distance = (proposed_position - other_move->position).length();
        
        if (actual_distance < min_distance) {
            return true;  // Collision detected
        }
    }
    
    return false;  // No collision
}

void MovementSystem::process_completed_paths(const SystemContext& ctx) {
    std::optional<PathResult> result = ctx.navigation_service->GetResult();
    while (result) {
        Entity* entity = ctx.entity_manager.get_entity(result->entity_id);
        if (!entity || !entity->has_component<PathfindingComponent>()) {
            result = ctx.navigation_service->GetResult();
            continue;
        }
        
        PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
        pathfinding->waypoints = result->path;
        pathfinding->waiting_for_path = false;

        result = ctx.navigation_service->GetResult();
    }
}

void MovementSystem::request_new_path(Entity& entity, Vec2 target, NavigationService* navigation_service) {
    if (!navigation_service) {
        return;
    }
    
    Movement* movement = entity.get_component<Movement>();
    PathfindingComponent* pathfinding = entity.get_component<PathfindingComponent>();
    
    PathRequest request;
    request.entity_id = entity.get_id();
    request.current_position = movement->position;
    request.destination = target;
    request.entity_pathing_radius = 0.5f;
    // Prioritize if this entity is player-controlled
    request.is_player_input = entity.has_component<PlayerOwnedComponent>();
    
    if (navigation_service->MakeRequest(request)) {
        pathfinding->waypoints.clear();
        pathfinding->waiting_for_path = true;
    }
}