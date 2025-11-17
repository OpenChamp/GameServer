#include "movement_system.hpp"
#include "entity_manager.hpp"
#include "services/navigation_service.hpp"
#include <components/pathfinding.hpp>
#include <components/stats.hpp>
#include <components/entity_state.hpp>
#include <log.hpp>
#include <cmath>

void MovementSystem::update(const SystemContext& ctx) {
    // Process pathfinding results for waiting entities
    if (ctx.navigation_service) {
        std::optional<PathResult> result = ctx.navigation_service->GetResult();
        while (result) {
            Entity* entity = ctx.entity_manager.get_entity(result->entity_id);
            LOG_INFO("Got pathfinding result for entity %u, path size=%zu", result->entity_id, result->path.size());
            if (entity && entity->has_component<PathfindingComponent>()) {
                PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
                EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();
                
                pathfinding->waypoints = result->path;
                pathfinding->current_waypoint_index = 0;
                
                if (result->path.empty()) {
                    LOG_WARN("Entity %u received EMPTY path!", entity->get_id());
                } else {
                    LOG_INFO("Entity %u received path with %zu waypoints, transitioning to MOVING", 
                             entity->get_id(), result->path.size());
                    // Transition to MOVING state when path is received
                    if (entity_state) {
                        entity_state->current_state = EntityState::MOVING;
                        entity_state->state_duration_ms = 0.0f;
                        LOG_INFO("Entity %u state transitioned to MOVING (state=%d)", entity->get_id(), (int)entity_state->current_state);
                    }
                }
            }
            result = ctx.navigation_service->GetResult();
        }
    }
    
    // Retry pending pathfinding requests (entities waiting for paths)
    if (ctx.navigation_service) {
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

    // Get all entities with movement components
    auto moving_entities = ctx.entity_manager.get_entities_with_component<Movement>();
    
    for (auto* entity : moving_entities) {
        Movement* movement = entity->get_component<Movement>();
        PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
        Stats* stats = entity->get_component<Stats>();
        
        if (!movement || !stats) {
            continue;
        }

        // Update stuck detection for entities with pathfinding
        if (pathfinding) {
            float delta_time_ms = ctx.delta_time_ms;
            EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();            
            // Check if entity has moved since last frame
            float movement_distance = distance(movement->position, pathfinding->last_position);
            
            if (movement_distance < 0.01f) {
                // Entity hasn't moved - accumulate stuck time
                pathfinding->stuck_time_ms += delta_time_ms;
                
                // If stuck too long and not already waiting for path, request new one
                if (pathfinding->stuck_time_ms >= PathfindingComponent::STUCK_THRESHOLD_MS && 
                    entity_state && entity_state->current_state == EntityState::MOVING &&
                    !pathfinding->waypoints.empty()) {
                    LOG_DEBUG("Minion %u is stuck, requesting new path", entity->get_id());
                    if (ctx.navigation_service && ctx.map && pathfinding->target_spawnpoint_id < ctx.map->spawnpoints.size()) {
                        request_new_path(*entity, pathfinding->target_spawnpoint_id, ctx.navigation_service, ctx.map);
                        // Transition to waiting state
                        if (entity_state) {
                            entity_state->current_state = EntityState::PATHFINDING_WAITING;
                            entity_state->state_duration_ms = 0.0f;
                        }
                    }
                    pathfinding->waypoints.clear();
                    pathfinding->current_waypoint_index = 0;
                }
            } else {
                // Entity moved - reset stuck timer
                pathfinding->stuck_time_ms = 0.0f;
            }
            
            // Update last position for next frame
            pathfinding->last_position = movement->position;
        }
        
        // If entity has pathfinding, move along the path (but not while waiting for new path)
        if (pathfinding && !pathfinding->waypoints.empty()) {
            EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();
            
            // Only move if in MOVING state
            if (!entity_state || entity_state->current_state != EntityState::MOVING) {
                continue;
            }
            
            // Get current waypoint
            if (pathfinding->current_waypoint_index >= pathfinding->waypoints.size()) {
                // Reached end of path - request path to next spawnpoint if possible
                if (ctx.map && pathfinding->target_spawnpoint_id < ctx.map->spawnpoints.size()) {
                    uint32_t next_spawnpoint = (pathfinding->target_spawnpoint_id + 1) % ctx.map->spawnpoints.size();
                    if (ctx.navigation_service && entity_state) {
                        request_new_path(*entity, next_spawnpoint, ctx.navigation_service, ctx.map);
                        entity_state->current_state = EntityState::PATHFINDING_WAITING;
                        entity_state->state_duration_ms = 0.0f;
                    }
                }
                pathfinding->waypoints.clear();
                pathfinding->current_waypoint_index = 0;
                continue;
            }
            
            Vec3 current_waypoint = pathfinding->waypoints[pathfinding->current_waypoint_index];
            Vec2 waypoint_2d = Vec2(current_waypoint.x, current_waypoint.z);
            Vec2 current_pos = movement->position;
            
            // Calculate direction to waypoint
            Vec2 direction = waypoint_2d - current_pos;
            float distance = direction.length();
            
            // Waypoint threshold for progress
            constexpr float WAYPOINT_THRESHOLD = 0.5f;
            
            if (distance < WAYPOINT_THRESHOLD) {
                // Reached waypoint, move to next one
                pathfinding->current_waypoint_index++;
                continue;
            }
            
            // Normalize direction and apply speed
            float move_speed = stats->move_speed;
            float distance_to_move = move_speed * (ctx.delta_time_ms / 1000.0f);
            
            Vec2 new_position;
            if (distance_to_move >= distance) {
                // Move directly to waypoint
                new_position = waypoint_2d;
            } else {
                // Move towards waypoint
                Vec2 normalized = direction.normalized();
                new_position = current_pos + (normalized * distance_to_move);
            }
            
            // Check for collision with other entities
            if (!has_collision(*entity, new_position, ctx.entity_manager)) {
                movement->position = new_position;
            } else {
                // If direct path is blocked, try to move sideways to avoid collision
                // TODO: Make this better with proper pathfinding around obstacles
                // Maybe move to nearest point along a circle around the obstacle, then recalculate path?
                Vec2 perpendicular = Vec2(-direction.y, direction.x).normalized();
                Vec2 sideways_left = current_pos + (perpendicular * distance_to_move);
                Vec2 sideways_right = current_pos + (perpendicular * -distance_to_move);
                
                if (!has_collision(*entity, sideways_left, ctx.entity_manager)) {
                    movement->position = sideways_left;
                } else if (!has_collision(*entity, sideways_right, ctx.entity_manager)) {
                    movement->position = sideways_right;
                }
                // If both sideways moves are blocked, just don't move (will accumulate stuck time)
            }
        }
    }
}

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
        
        float actual_distance = distance(proposed_position, other_move->position);
        
        if (actual_distance < min_distance) {
            return true;  // Collision detected
        }
    }
    
    return false;  // No collision
}

float MovementSystem::distance(const Vec2& a, const Vec2& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
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
        LOG_DEBUG("Minion %u requested path to spawnpoint %u", entity.get_id(), target_spawnpoint_id);
    }
}
