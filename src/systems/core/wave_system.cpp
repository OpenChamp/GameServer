#include <libs/log.hpp>
#include <systems/core/wave_system.hpp>
#include <systems/entity_manager.hpp>
#include <systems/util/serialization_system.hpp>
#include <services/network_service.hpp>
#include <services/navigation_service.hpp>
#include <components/movement.hpp>
#include <components/pathfinding.hpp>
#include <components/entity_state.hpp>
#include <vector>
#include <string>
#include <cmath>
#include <components/network_entity.hpp>

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
    if (elapsed_time_ms >= (wave_index == 0 ? first_wave_delay_ms : wave_interval_ms)) {
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
    
    // Get spawn position from map
    Vec2 spawn_pos = map_->spawnpoints[spawn_point_id];
    
    // Spawn entity using SpawningSystem
    // (SpawningSystem will handle collision checking and finding free space)
    EntityID minion_id = spawning_system_.spawn_entity_from_template(ctx, minion_template, spawn_pos, team_id);
    if (minion_id == INVALID_ENTITY_ID) {
        LOG_ERROR("Failed to spawn minion of type %s", minion_template.c_str());
        return false;
    }
    return true;
}

