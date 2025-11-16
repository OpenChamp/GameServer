#include <log.hpp>
#include "wave_system.hpp"
#include "entity_manager.hpp"
#include "serialization_system.hpp"
#include "services/network_service.hpp"
#include <components/movement.hpp>
#include <vector>
#include <string>

WaveSystem::WaveSystem(EntityManager* entity_manager, NetworkService* network_service) {
    // Constructor can initialize state if needed
    entity_manager_ = entity_manager;
    network_service_ = network_service;
    wave_interval_ms = 30000.0f; // 30 seconds
    wave_delay_ms = 1000.0f;     // 1 second
    elapsed_time_ms = 0.0f;
    last_spawn_timestamp = 0.0f;
    minion_index = 0;
    wave_index = 0;
    special_wave_offset = 5;
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
    // Minion Spawning
    if(minion_index > 0) {
        if (elapsed_time_ms - last_spawn_timestamp >= wave_delay_ms) {
            // Spawn next minion
            LOG_INFO("Spawning new minion from wave %d, index %d", wave_index, minion_index);
            for(uint8_t i = 1; i < 3; i++) {
                create_minion(
                    (wave_index % special_wave_offset == 0 ? special_minion_wave : default_minion_wave)[minion_index - 1], // since we use 0 for initial state
                    i // Default team ID
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


bool WaveSystem::create_minion(const std::string& minion_template, uint8_t team_id) {
    Entity& minion = entity_manager_->create_entity_from_template(minion_template);
    if (minion.get_id() == 0) {
        LOG_ERROR("Failed to spawn minion of type %s", minion_template.c_str());
        return false;
    }

    Stats* stats = minion.get_component<Stats>();
    if (stats) {
        stats->team_id = team_id;
    }
    
    // Broadcast minion spawn to all clients
    if (network_service_) {
        Movement* move_comp = minion.get_component<Movement>();
        if (move_comp) {
            std::vector<uint8_t> packet = SerializationSystem::serialize_entity_spawn(minion.get_id(), move_comp->position, team_id, minion_template);
            network_service_->broadcast_packet(packet);
        } else {
            LOG_WARN("Spawned minion %u has no Movement component", minion.get_id());
        }
    }
    
    LOG_INFO("Spawned minion with ID %u for team %d", minion.get_id(), team_id);
    return true;
}
