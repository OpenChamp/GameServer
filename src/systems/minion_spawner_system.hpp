#pragma once

#include "entity_manager.hpp"
#include "components/game_state.hpp"

/**
 * System to spawn minions at regular intervals.
 * Spawns one minion every second, alternating between spawnpoints.
 */
class MinionSpawnerSystem {
public:
    MinionSpawnerSystem();
    
    /**
     * Update the spawner system.
     * Spawns minions at regular intervals (1 second) only if game is ongoing.
     * @param entity_manager Reference to the entity manager
     * @param delta_time Time elapsed since last update in seconds
     * @param map_entity Pointer to the map entity (contains NavMesh)
     * @param game_state Current game state (only spawn if ONGOING)
     */
    void update(EntityManager& entity_manager, float delta_time, const Entity* map_entity, GAME_STATE game_state);
    
    /**
     * Get the number of minions currently spawned.
     * @return Count of active minions
     */
    int get_minion_count() const;
    
private:
    float spawn_timer_ = 0.0f;
    const float SPAWN_INTERVAL_ = 1.0f;  // 1 second between spawns
    int spawn_spawnpoint_index_ = 0;      // Alternate between spawnpoints
};
