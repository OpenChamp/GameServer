#pragma once

#include <services/network_service.hpp>
#include <services/navigation_service.hpp>
#include <cstdint>
#include <systems/entity_manager.hpp>
#include <components/map.hpp>

/**
 * System to manage wave spawning and progression.
 * Handles the timing and logic for enemy waves in the game.
 * Minions spawn and automatically request paths to their target spawnpoints.
 */
class WaveSystem {
public:
    WaveSystem(EntityManager* entity_manager, NetworkService* network_service = nullptr, NavigationService* navigation_service = nullptr, const Map* map = nullptr);
    /** 
     * Tick the wave system to update wave state and process pathfinding results.
     * @param delta_time_ms Time elapsed since last tick in milliseconds
     */
    void tick(float delta_time_ms);
    
private:
    EntityManager* entity_manager_;
    NetworkService* network_service_;
    NavigationService* navigation_service_;
    const Map* map_;
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
     * @param minion_template The template name of the minion to spawn
     * @param team_id The team this minion belongs to
     * @param spawn_point_id Starting spawnpoint ID (0-indexed)
     * @return true if minion spawned successfully
     */
    bool create_minion(const std::string& minion_template, uint8_t team_id, uint32_t spawn_point_id);
    
    /**
     * Request a path for a minion to its target spawnpoint.
     * @param entity Entity with Movement and PathfindingComponent
     * @param target_spawnpoint_id Target spawnpoint ID
     */
    void request_minion_path(Entity& entity, uint32_t target_spawnpoint_id);
    
    /**
     * Find a free spawn position near the given spawnpoint.
     * Checks if there are other entities at the spawnpoint and offsets position if needed.
     * @param spawn_point_id The spawnpoint index
     * @param collision_radius The radius to check for collisions
     * @return A free position near the spawnpoint
     */
    Vec2 find_free_spawn_position(uint32_t spawn_point_id, float collision_radius);
};
