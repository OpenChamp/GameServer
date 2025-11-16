#pragma once

#include <services/network_service.hpp>
#include <cstdint>
#include <systems/entity_manager.hpp>

/**
 * System to manage wave spawning and progression.
 * Handles the timing and logic for enemy waves in the game.
 */
class WaveSystem {
public:
    WaveSystem(EntityManager* entity_manager, NetworkService* network_service = nullptr);
    /** 
     * Tick the wave system to update wave state.
     * @param delta_time_ms Time elapsed since last tick in milliseconds
     */
    void tick(float delta_time_ms);
    
private:
    EntityManager* entity_manager_;
    NetworkService* network_service_;
    float wave_interval_ms; // 30 seconds between waves
    float wave_delay_ms;    // 1 second delay between minions in a wave
    float elapsed_time_ms;
    float last_spawn_timestamp;
    int minion_index;
    int wave_index;
    int special_wave_offset;
    std::vector<std::string> default_minion_wave;
    std::vector<std::string> special_minion_wave;

    /**
     * Try to spawn a minion of the given type.
     * @param minion_type_id The type ID of the minion to spawn
     * @return true if minion spawned successfully
     */
    bool create_minion(const std::string& minion_template, uint8_t team_id);
};
