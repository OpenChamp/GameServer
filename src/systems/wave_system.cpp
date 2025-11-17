#include <log.hpp>
#include "wave_system.hpp"
#include "entity_manager.hpp"
#include "serialization_system.hpp"
#include "services/network_service.hpp"
#include "services/navigation_service.hpp"
#include <components/movement.hpp>
#include <components/pathfinding.hpp>
#include <components/entity_state.hpp>
#include <vector>
#include <string>
#include <cmath>

WaveSystem::WaveSystem(EntityManager* entity_manager, NetworkService* network_service, NavigationService* navigation_service, const Map* map) {
    entity_manager_ = entity_manager;
    network_service_ = network_service;
    navigation_service_ = navigation_service;
    map_ = map;
    wave_interval_ms = 30000.0f;
    wave_delay_ms = 1000.0f;
    elapsed_time_ms = 0.0f;
    last_spawn_timestamp = 0.0f;
    minion_index = 0;
    wave_index = 0;
    special_wave_offset = 5;

    // TODO: Load from config file mode -- cmkrist 11/16/2025
    default_minion_wave = {
        "melee_minion",
        "melee_minion",
        "melee_minion",
        "ranged_minion",
        "magic_minion"
    };
    special_minion_wave = {
        "melee_minion",
        "melee_minion",
        "melee_minion",
        "cannon_minion",
        "ranged_minion",
        "magic_minion",
        "ranged_minion"
    };
}

void WaveSystem::tick(float delta_time_ms) {
    elapsed_time_ms += delta_time_ms;
    // Process pathfinding results for waiting minions
    if (navigation_service_) {
        std::optional<PathResult> result = navigation_service_->GetResult();
        while (result) {
            Entity* entity = entity_manager_->get_entity(result->entity_id);
            if (entity && entity->has_component<PathfindingComponent>()) {
                PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
                EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();
                
                pathfinding->waypoints = result->path;
                pathfinding->current_waypoint_index = 0;
                
                if (result->path.empty()) {
                    LOG_WARN("Minion %u received EMPTY path!", entity->get_id());
                } else {
                    LOG_DEBUG("Minion %u received path with %zu waypoints", 
                             entity->get_id(), result->path.size());
                    // Transition to MOVING state when path is received
                    if (entity_state) {
                        entity_state->current_state = EntityState::MOVING;
                        entity_state->state_duration_ms = 0.0f;
                    }
                }
            }
            result = navigation_service_->GetResult();
        }
    }
    
    // Retry pending pathfinding requests (entities waiting for paths)

    if (navigation_service_) {
        auto entities = entity_manager_->get_entities_with_component<PathfindingComponent>();
        for (auto* entity : entities) {
            PathfindingComponent* pathfinding = entity->get_component<PathfindingComponent>();
            EntityStateComponent* entity_state = entity->get_component<EntityStateComponent>();
            if (pathfinding && entity_state && 
                entity_state->current_state == EntityState::PATHFINDING_WAITING && 
                pathfinding->waypoints.empty()) {
                request_minion_path(*entity, pathfinding->target_spawnpoint_id);
            }
        }
    }
    
    // Minion Spawning
    if(minion_index > 0) {
        if (elapsed_time_ms - last_spawn_timestamp >= wave_delay_ms) {
            LOG_INFO("Spawning new minion from wave %d, index %d", wave_index, minion_index);
            for(uint8_t team_id = 1; team_id < 3; team_id++) {
                create_minion(
                    (wave_index % special_wave_offset == 0 ? special_minion_wave : default_minion_wave)[minion_index - 1],
                    team_id,
                    team_id - 1  // Spawn from team-specific spawnpoint
                );
            }
            last_spawn_timestamp = elapsed_time_ms;
            // Overflow check
            if (minion_index >= (wave_index % special_wave_offset == 0 ? special_minion_wave.size() : default_minion_wave.size())) {
                minion_index = 0;
            } else {
                minion_index++;
            }
        }
    }
    // Wave Spawning
    if (elapsed_time_ms >= wave_interval_ms) {
        LOG_INFO("Spawning new wave");
        minion_index = 1;
        wave_index++;
        elapsed_time_ms = 0;
        last_spawn_timestamp = 0;
    }
}

bool WaveSystem::create_minion(const std::string& minion_template, uint8_t team_id, uint32_t spawn_point_id) {
    Entity& minion = entity_manager_->create_entity_from_template(minion_template);
    if (minion.get_id() == 0) {
        LOG_ERROR("Failed to spawn minion of type %s", minion_template.c_str());
        return false;
    }

    // Set team ID
    Stats* stats = minion.get_component<Stats>();
    if (stats) {
        stats->team_id = team_id;
    }
    if (map_->spawnpoints.size() == 0) {
        LOG_WARN("Map has no spawnpoints defined, minion spawn position may be invalid");
    }
    // Set Spawn
    Movement* move_comp = minion.get_component<Movement>();
    if (move_comp && map_ && spawn_point_id < map_->spawnpoints.size()) {
        move_comp->position = find_free_spawn_position(spawn_point_id, move_comp->collision_radius);
    }
    
    // Add pathfinding component
    auto pathfinding = std::make_unique<PathfindingComponent>();
    minion.add_component(std::move(pathfinding));
    
    // Add entity state component (starts in SPAWNED state)
    auto entity_state = std::make_unique<EntityStateComponent>();
    entity_state->current_state = EntityState::SPAWNED;
    minion.add_component(std::move(entity_state));
    
    // Request initial path to next spawnpoint (enemy spawn)
    // TODO: Make this better -- cmkrist 16/11/2025
    uint32_t target_spawnpoint = (spawn_point_id + 1) % (map_ ? map_->spawnpoints.size() : 2);
    request_minion_path(minion, target_spawnpoint);
    
    // Notify clients of the new entity
    // README: Not handled by NetworkSyncSystem (Manages ongoing entities only) -- cmkrist 16/11/2025
    if (network_service_ && move_comp) {
        std::vector<uint8_t> packet = SerializationSystem::serialize_entity_spawn(minion.get_id(), move_comp->position, team_id, minion_template);
        network_service_->broadcast_packet(packet);
    }
    
    LOG_INFO("Spawned minion with ID %u for team %d from spawnpoint %u -> %u", minion.get_id(), team_id, spawn_point_id, target_spawnpoint);
    return true;
}

void WaveSystem::request_minion_path(Entity& entity, uint32_t target_spawnpoint_id) {
    if (!navigation_service_ || !map_ || target_spawnpoint_id >= map_->spawnpoints.size()) {
        LOG_WARN("Cannot request path: navigation_service=%p, map=%p, spawnpoint_id=%u", navigation_service_, map_, target_spawnpoint_id);
        return;
    }
    
    Movement* move = entity.get_component<Movement>();
    PathfindingComponent* pathfinding = entity.get_component<PathfindingComponent>();
    
    if (!move || !pathfinding) {
        LOG_WARN("Minion %u missing Movement or PathfindingComponent", entity.get_id());
        return;
    }
    
    Vec3 start = Vec3(move->position.x, 0.0f, move->position.y);
    Vec2 goal_2d = map_->spawnpoints[target_spawnpoint_id];
    Vec3 goal = Vec3(goal_2d.x, 0.0f, goal_2d.y);
    
    LOG_DEBUG("Minion %u requesting path from (%.1f, %.1f) to spawnpoint %u (%.1f, %.1f)", 
             entity.get_id(), start.x, start.z, target_spawnpoint_id, goal.x, goal.z);
    
    PathRequest request;
    request.entity_id = entity.get_id();
    request.current_position = start;
    request.destination = goal;
    request.entity_pathing_radius = 0.5f;
    
    EntityStateComponent* entity_state = entity.get_component<EntityStateComponent>();
    
    if (navigation_service_->MakeRequest(request)) {
        pathfinding->target_spawnpoint_id = target_spawnpoint_id;
        // Transition to waiting state
        if (entity_state) {
            entity_state->current_state = EntityState::PATHFINDING_WAITING;
            entity_state->state_duration_ms = 0.0f;
        }
        LOG_DEBUG("Path request queued for minion %u", entity.get_id());
    } else {
        // Mark as waiting for path so we retry next tick
        pathfinding->target_spawnpoint_id = target_spawnpoint_id;
        if (entity_state) {
            entity_state->current_state = EntityState::PATHFINDING_WAITING;
            entity_state->state_duration_ms = 0.0f;
        }
        LOG_DEBUG("Pathfinding queue full, will retry for minion %u", entity.get_id());
    }
}

Vec2 WaveSystem::find_free_spawn_position(uint32_t spawn_point_id, float collision_radius) {
    if (!map_ || spawn_point_id >= map_->spawnpoints.size()) {
        // Fallback to default spawnpoint if invalid
        return Vec2(0.0f, 0.0f);
    }
    
    Vec2 base_position = map_->spawnpoints[spawn_point_id];
    
    // Get all entities with movement components to check for collisions
    auto moving_entities = entity_manager_->get_entities_with_component<Movement>();
    
    // Check if base position is free
    bool position_free = true;
    for (auto* entity : moving_entities) {
        const Movement* other_move = entity->get_component<Movement>();
        if (other_move) {
            float dx = base_position.x - other_move->position.x;
            float dy = base_position.y - other_move->position.y;
            float distance = std::sqrt(dx * dx + dy * dy);
            float min_distance = collision_radius + other_move->collision_radius;
            
            if (distance < min_distance) {
                position_free = false;
                break;
            }
        }
    }
    
    if (position_free) {
        return base_position;
    }
    
    // If base position is occupied, try to find a free spot nearby
    // Use expanding circles to search for free space
    constexpr float SPAWN_SEARCH_RADIUS = 5.0f;
    constexpr int SEARCH_SAMPLES = 16;  // Number of angles to check
    
    for (float search_distance = collision_radius * 2.0f; search_distance <= SPAWN_SEARCH_RADIUS; search_distance += 0.5f) {
        for (int i = 0; i < SEARCH_SAMPLES; ++i) {
            float angle = (2.0f * 3.14159265f * i) / SEARCH_SAMPLES;
            Vec2 candidate = base_position + Vec2(std::cos(angle) * search_distance, std::sin(angle) * search_distance);
            
            // Check if this candidate position is free
            bool candidate_free = true;
            for (auto* entity : moving_entities) {
                const Movement* other_move = entity->get_component<Movement>();
                if (other_move) {
                    float dx = candidate.x - other_move->position.x;
                    float dy = candidate.y - other_move->position.y;
                    float distance = std::sqrt(dx * dx + dy * dy);
                    float min_distance = collision_radius + other_move->collision_radius;
                    
                    if (distance < min_distance) {
                        candidate_free = false;
                        break;
                    }
                }
            }
            
            if (candidate_free) {
                return candidate;
            }
        }
    }
    
    // If no free space found after search, return base position anyway
    // (let collision detection handle it later? push everyone back?(not-implemented(tm))) -- cmkrist 16/11/2025
    LOG_WARN("Could not find free spawn position for spawnpoint %u, using base position", spawn_point_id);
    return base_position;
}

