#pragma once
#include <cstdint>

#include <systems/system_context.hpp>
#include <systems/entity_manager.hpp>
#include <systems/core/spawning_system.hpp>
#include <services/network_service.hpp>
#include <services/navigation_service.hpp>
#include <components/map.hpp>

/**
 * WaveSystem - Manages enemy wave spawning and progression
 * 
 * RESPONSIBILITIES:
 *   - Spawn minions at regular intervals
 *   - Track wave progression
 *   - Process pathfinding results for spawned minions
 *   - Coordinate with SpawningSystem for entity creation
 * 
 * SYSTEMS USED:
 *   - SpawningSystem (for entity spawning)
 *   - NavigationService (for pathfinding)
 * 
 * SYSTEM INTERACTION:
 *   - Called by GameplayCoordinator via update()
 *   - Spawns entities at regular intervals
 *   - Requests paths from NavigationService
 * 
 * USAGE:
 *   WaveSystem wave_system;
 *   wave_system.initialize(entity_manager, network_service, navigation_service, map);
 *   // In game loop:
 *   wave_system.update(ctx);
 */
class WaveSystem {
public:
    WaveSystem() = default;
    
    /**
     * Initialize the wave system with required services.
     * Must be called before update().
     * @param entity_manager Entity manager for spawning
     * @param network_service Network service for broadcasting spawns
     * @param navigation_service Navigation service for pathfinding
     * @param map Map data for spawnpoints
     */
    void initialize(EntityManager* entity_manager, NetworkService* network_service,
                   NavigationService* navigation_service, const Map* map);
    
    /**
     * Update wave system - spawn minions and process pathfinding results.
     * @param ctx System context
     */
    void update(const SystemContext& ctx);
    
private:
    EntityManager* entity_manager_ = nullptr;
    NetworkService* network_service_ = nullptr;
    NavigationService* navigation_service_ = nullptr;
    const Map* map_ = nullptr;
    SpawningSystem spawning_system_;
    float wave_interval_ms = 30000.0f;  // 30 seconds between waves (debug timing)
    float first_wave_delay_ms = 1000.0f; // 1 second before first wave spawns (debug timing - reduced for testing)
    float wave_delay_ms = 100.0f;      // 100ms delay between minions in a wave (debug timing - reduced for testing)
    float elapsed_time_ms;
    float last_spawn_timestamp;
    int minion_index;
    int wave_index;
    int special_wave_offset;
    std::vector<std::string> default_minion_wave;
    std::vector<std::string> special_minion_wave;

    /**
     * Try to spawn a minion of the given type.
     * @param ctx System context (contains entity manager and network service)
     * @param minion_template The template name of the minion to spawn
     * @param team_id The team this minion belongs to
     * @param spawn_point_id Starting spawnpoint ID (0-indexed)
     * @return true if minion spawned successfully
     */
    bool create_minion(const SystemContext& ctx, const std::string& minion_template, uint8_t team_id, uint32_t spawn_point_id);
};