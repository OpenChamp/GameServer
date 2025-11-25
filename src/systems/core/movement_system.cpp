#include <systems/core/movement_system.hpp>
#include <systems/core/collision_system.hpp>
#include <systems/entity_manager.hpp>
#include <services/navigation_service.hpp>
#include <components/pathfinding.hpp>
#include <components/stats.hpp>
#include <components/entity_state.hpp>
#include <components/auto_attack.hpp>
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
    // Phase 1: Process pathfinding results and retry requests
    process_pathfinding_phase(ctx);
    
    // Phase 2: Update all moving entities
    auto moving_entities = ctx.entity_manager.get_entities_with_component<Movement>();
    for (auto* entity : moving_entities) {
        update_entity_movement(ctx, *entity);
    }
}

void MovementSystem::process_pathfinding_phase(const SystemContext& ctx) {
    if (!ctx.navigation_service) return;
    
    // Process completed pathfinding results
    process_completed_paths(ctx);
    
    // Retry pending pathfinding requests
    retry_stuck_entities(ctx);
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
        EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();
        
        pathfinding->waypoints = result->path;
        pathfinding->current_waypoint_index = 0;
        
        if (result->path.empty()) {
            LOG_WARN("Entity %u received EMPTY path!", entity->get_id());
        } else {
            if (entity_state) {
                entity_state->current_state = EntityState::MOVING;
                entity_state->state_duration_ms = 0.0f;
            }
        }
        
        result = ctx.navigation_service->GetResult();
    }
}

void MovementSystem::retry_stuck_entities(const SystemContext& ctx) {
    auto entities = ctx.entity_manager.get_entities_with_component<PathfindingComponent>();
    for (auto* entity : entities) {
        PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
        EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();
        
        if (pathfinding && entity_state && 
            entity_state->current_state == EntityState::PATHFINDING_WAITING && 
            pathfinding->waypoints.empty()) {
            request_new_path(*entity, pathfinding->target_spawnpoint_id, ctx.navigation_service, ctx.map);
        }
    }
}

void MovementSystem::update_entity_movement(const SystemContext& ctx, Entity& entity) {
    Movement* movement = entity.get_component<Movement>();
    PathfindingComponent* pathfinding = entity.get_component<PathfindingComponent>();
    Stats* stats = entity.get_component<Stats>();
    EntityStateComponent* state_comp = entity.get_component<EntityStateComponent>();
    
    if (!movement || !stats) return;
    
    // If holding for target, move toward the target entity
    if (state_comp && state_comp->current_state == EntityState::HOLDING_FOR_TARGET &&
        state_comp->target_entity_id != INVALID_ENTITY_ID) {
        Entity* target = ctx.entity_manager.get_entity(state_comp->target_entity_id);
        if (target) {
            Movement* target_movement = target->get_component<Movement>();
            if (target_movement) {
                // Move toward target
                Vec2 direction = target_movement->position - movement->position;
                float distance = direction.length();
                
                if (distance > 0.1f) {
                    Vec2 normalized_dir = direction.normalized();
                    float move_speed = stats->move_speed;
                    float distance_to_move = move_speed * (ctx.delta_time_ms / 1000.0f);
                    movement->position = movement->position + (normalized_dir * distance_to_move);
                }
            }
        }
        return;  // Don't process other movement
    }
    
    // Handle movement during ATTACKING state (kiting)
    if (state_comp && state_comp->current_state == EntityState::ATTACKING) {
        // Check if entity has AutoAttackComponent and kiting is enabled
        auto* auto_attack = entity.get_component<AutoAttackComponent>();
        if (auto_attack && auto_attack->allow_kiting && auto_attack->kite_while_attacking) {
            // Move toward target with reduced speed
            if (state_comp->target_entity_id != INVALID_ENTITY_ID) {
                Entity* target = ctx.entity_manager.get_entity(state_comp->target_entity_id);
                if (target) {
                    Movement* target_movement = target->get_component<Movement>();
                    if (target_movement) {
                        // Move toward target at reduced speed
                        Vec2 direction = target_movement->position - movement->position;
                        float distance = direction.length();
                        
                        if (distance > 0.1f) {
                            Vec2 normalized_dir = direction.normalized();
                            float base_move_speed = stats->move_speed * auto_attack->movement_during_attack;
                            float distance_to_move = base_move_speed * (ctx.delta_time_ms / 1000.0f);
                            movement->position = movement->position + (normalized_dir * distance_to_move);
                        }
                    }
                }
            }
        }
        return;  // Stop other movement processing during attack
    }
    
    // Update direct movement targets (players, point-based movement)
    if (state_comp && state_comp->current_state == EntityState::MOVING && 
        !state_comp->target_positions.empty()) {
        update_direct_movement(ctx, entity, *movement, *stats, *state_comp);
        return;
    }
    
    // Update pathfinding entities (minions following waypoints)
    // TODO: Differentiate between minions and other pathfinding entities (AI Component?) -- cmkrist 18/11/2025
    if (pathfinding) {
        update_stuck_detection(ctx, entity, *movement, *pathfinding, state_comp);
        update_waypoint_movement(ctx, entity, *movement, *stats, *pathfinding, state_comp);
    }
}

void MovementSystem::update_direct_movement(const SystemContext& ctx, Entity& entity, 
                                           Movement& movement, Stats& stats, EntityStateComponent& state_comp) {
    Vec2 current_pos = movement.position;
    Vec2 target_pos = state_comp.target_positions.at(0);
    Vec2 direction = target_pos - current_pos;
    float distance = direction.length();
    
    // Check if reached target
    if (distance < TARGET_THRESHOLD) {
        state_comp.target_positions.erase(state_comp.target_positions.begin());
        
        // Transition to IDLE when all targets are reached
        // Separate system? -- cmkrist 18/11/2025
        if (state_comp.target_positions.empty()) {
            state_comp.current_state = EntityState::IDLE;
            state_comp.state_duration_ms = 0.0f;
            LOG_DEBUG("Entity %u reached destination, transitioning to IDLE", entity.get_id());
        }
        return;
    }
    
    // Calculate new position based on speed
    float move_speed = stats.move_speed;
    float distance_to_move = move_speed * (ctx.delta_time_ms / 1000.0f);
    
    Vec2 new_position = (distance_to_move >= distance) 
        ? target_pos 
        : current_pos + (direction.normalized() * distance_to_move);
    
    movement.position = new_position;
}

void MovementSystem::update_stuck_detection(const SystemContext& ctx, Entity& entity, 
                                           Movement& movement, PathfindingComponent& pathfinding, 
                                           EntityStateComponent* entity_state) {
    float movement_distance = (movement.position - pathfinding.last_position).length();
    
    if (movement_distance < STUCK_DETECTION_THRESHOLD) {
        // Entity hasn't moved - accumulate stuck time
        pathfinding.stuck_time_ms += ctx.delta_time_ms;
        
        // Check if stuck long enough to request new path
        if (pathfinding.stuck_time_ms >= PathfindingComponent::STUCK_THRESHOLD_MS && 
            entity_state && entity_state->current_state == EntityState::MOVING &&
            !pathfinding.waypoints.empty() && ctx.navigation_service && ctx.map &&
            pathfinding.target_spawnpoint_id < ctx.map->spawnpoints.size()) {
            
            LOG_DEBUG("Entity %u is stuck, requesting new path", entity.get_id());
            request_new_path(entity, pathfinding.target_spawnpoint_id, ctx.navigation_service, ctx.map);
            
            if (entity_state) {
                entity_state->current_state = EntityState::PATHFINDING_WAITING;
                entity_state->state_duration_ms = 0.0f;
            }
            pathfinding.waypoints.clear();
            pathfinding.current_waypoint_index = 0;
        }
    } else {
        // Entity moved - reset stuck timer
        pathfinding.stuck_time_ms = 0.0f;
    }
    
    // Update last position for next frame
    pathfinding.last_position = movement.position;
}

void MovementSystem::update_waypoint_movement(const SystemContext& ctx, Entity& entity, 
                                             Movement& movement, Stats& stats, 
                                             PathfindingComponent& pathfinding, 
                                             EntityStateComponent* entity_state) {
    // Skip if waiting for pathfinding or no waypoints
    if (!pathfinding.waypoints.size() || 
        !entity_state || entity_state->current_state != EntityState::MOVING) {
        return;
    }
    
    // Check if reached end of path
    if (pathfinding.current_waypoint_index >= pathfinding.waypoints.size()) {
        request_path_to_next_spawnpoint(ctx, entity, pathfinding, entity_state);
        return;
    }
    
    // Move toward current waypoint
    Vec2 current_waypoint = pathfinding.waypoints[pathfinding.current_waypoint_index];
    Vec2 current_pos = movement.position;
    Vec2 direction = current_waypoint - current_pos;
    float distance = direction.length();
    
    // Check if reached waypoint
    if (distance < WAYPOINT_THRESHOLD) {
        pathfinding.current_waypoint_index++;
        return;
    }
    
    // Calculate new position based on speed
    float move_speed = stats.move_speed;
    float distance_to_move = move_speed * (ctx.delta_time_ms / 1000.0f);
    
    Vec2 new_position = (distance_to_move >= distance) 
        ? current_waypoint 
        : current_pos + (direction.normalized() * distance_to_move);
    
    movement.position = new_position;
}

void MovementSystem::request_path_to_next_spawnpoint(const SystemContext& ctx, Entity& entity, 
                                                    PathfindingComponent& pathfinding,
                                                    EntityStateComponent* entity_state) {
    if (!ctx.map || pathfinding.target_spawnpoint_id >= ctx.map->spawnpoints.size()) {
        return;
    }
    
    uint32_t next_spawnpoint = (pathfinding.target_spawnpoint_id + 1) % ctx.map->spawnpoints.size();
    if (ctx.navigation_service && entity_state) {
        request_new_path(entity, next_spawnpoint, ctx.navigation_service, ctx.map);
        entity_state->current_state = EntityState::PATHFINDING_WAITING;
        entity_state->state_duration_ms = 0.0f;
    }
    
    pathfinding.waypoints.clear();
    pathfinding.current_waypoint_index = 0;
}

// ============================================================================
// Utility Functions
// ============================================================================

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

void MovementSystem::request_new_path(Entity& entity, uint32_t target_spawnpoint_id, NavigationService* navigation_service, const Map* map) {
    if (!navigation_service || !map || target_spawnpoint_id >= map->spawnpoints.size()) {
        return;
    }
    
    Movement* movement = entity.get_component<Movement>();
    PathfindingComponent* pathfinding = entity.get_component<PathfindingComponent>();
    
    if (!movement || !pathfinding) {
        return;
    }
    
    Vec3 start = Vec3(movement->position.x, 0.0f, movement->position.y);
    Vec2 goal_2d = map->spawnpoints[target_spawnpoint_id];
    Vec3 goal = Vec3(goal_2d.x, 0.0f, goal_2d.y);
    
    PathRequest request;
    request.entity_id = entity.get_id();
    request.current_position = start;
    request.destination = goal;
    request.entity_pathing_radius = 0.5f;
    
    if (navigation_service->MakeRequest(request)) {
        EntityStateComponent* entity_state = entity.get_component<EntityStateComponent>();
        if (entity_state) {
            entity_state->current_state = EntityState::PATHFINDING_WAITING;
            entity_state->state_duration_ms = 0.0f;
        }
        pathfinding->target_spawnpoint_id = target_spawnpoint_id;
        LOG_DEBUG("Entity %u requested path to spawnpoint %u", entity.get_id(), target_spawnpoint_id);
    }
}