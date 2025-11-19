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
#include <network_entity.hpp>

void WaveSystem::initialize(EntityManager* entity_manager, NetworkService* network_service,
                           NavigationService* navigation_service, const Map* map) {
    entity_manager_ = entity_manager;
    network_service_ = network_service;
    navigation_service_ = navigation_service;
    map_ = map;
    elapsed_time_ms = 0.0f;
    last_spawn_timestamp = 0.0f;
    minion_index = 0;
    wave_index = 0;
    special_wave_offset = 5;

    // TODO: Load from config file -- cmkrist 11/16/2025
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

void WaveSystem::update(const SystemContext& ctx) {
    if (!entity_manager_) {
        return;
    }
    
    float delta_time_ms = ctx.delta_time_ms;
    elapsed_time_ms += delta_time_ms;
    
    // Minion Spawning
    if(minion_index > 0) {
        if (elapsed_time_ms - last_spawn_timestamp >= wave_delay_ms) {
            for(uint8_t team_id = 1; team_id < 3; team_id++) {
                create_minion(
                    ctx,
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

bool WaveSystem::create_minion(const SystemContext& ctx, const std::string& minion_template, uint8_t team_id, uint32_t spawn_point_id) {
    if (!map_ || spawn_point_id >= map_->spawnpoints.size()) {
        LOG_WARN("Invalid spawn point %u for minion", spawn_point_id);
        return false;
    }
    
    // Find spawn position
    Vec2 spawn_pos = map_->spawnpoints[spawn_point_id];
    
    // Get collision radius from template entity if available
    Entity& temp = ctx.entity_manager.create_entity_from_template(minion_template);
    float collision_radius = 0.5f;  // Default
    if (auto* move = temp.get_component<Movement>()) {
        collision_radius = move->collision_radius;
    }
    ctx.entity_manager.destroy_entity(temp.get_id());
    
    // Find free spawn position
    spawn_pos = find_free_spawn_position(spawn_point_id, collision_radius);
    
    // Spawn entity using SpawningSystem
    EntityID minion_id = spawning_system_.spawn_entity_from_template(ctx, minion_template, spawn_pos, team_id);
    if (minion_id == INVALID_ENTITY_ID) {
        LOG_ERROR("Failed to spawn minion of type %s", minion_template.c_str());
        return false;
    }
    
    Entity* minion = ctx.entity_manager.get_entity(minion_id);
    if (!minion) {
        return false;
    }
    
    // Add pathfinding component if not already present
    if (!minion->has_component<PathfindingComponent>()) {
        auto pathfinding = std::make_unique<PathfindingComponent>();
        minion->add_component(std::move(pathfinding));
    }
    
    // Add entity state component if not already present
    if (!minion->has_component<EntityStateComponent>()) {
        auto entity_state = std::make_unique<EntityStateComponent>();
        entity_state->current_state = EntityState::SPAWNED;
        minion->add_component(std::move(entity_state));
    }
    
    // Request initial path to next spawnpoint
    uint32_t target_spawnpoint = (spawn_point_id + 1) % map_->spawnpoints.size();
    request_minion_path(*minion, target_spawnpoint);
    
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

