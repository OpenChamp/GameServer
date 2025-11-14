#include "minion_spawner_system.hpp"
#include "components/navmesh.hpp"
#include "components/minion.hpp"
#include "components/movement.hpp"
#include "components/stats.hpp"
#include "components/game_state.hpp"
#include <log.hpp>
#include <systems/data_loader.hpp>

MinionSpawnerSystem::MinionSpawnerSystem()
    : spawn_timer_(0.0f)
    , spawn_spawnpoint_index_(0) {
    LOG_INFO("MinionSpawnerSystem initialized");
}

void MinionSpawnerSystem::update(EntityManager& entity_manager, float delta_time, const Entity* map_entity, GAME_STATE game_state) {
    // Only spawn minions when game is ongoing (lobby is full and game has started)
    if (game_state != GAME_STATE::ONGOING) {
        return;
    }
    
    if (!map_entity) {
        return;
    }
    
    // Get the navmesh component
    const NavMesh* navmesh = map_entity->get_component<NavMesh>();
    if (!navmesh || navmesh->spawnpoints.empty()) {
        return;
    }
    
    // Update spawn timer
    spawn_timer_ += delta_time;
    
    // Check if it's time to spawn
    if (spawn_timer_ >= SPAWN_INTERVAL_) {
        spawn_timer_ = 0.0f;
        
        // Create minion entity
        Entity& minion = entity_manager.create_entity_from_template("minion");
        
        // Set spawn position at current spawnpoint
        Vec3 spawn_pos = navmesh->spawnpoints[spawn_spawnpoint_index_];
        
        // Add Movement component
        auto movement = minion.get_component<Movement>();
        movement->position = spawn_pos;
        movement->is_moving = true;
        
        // Add Minion component
        auto minion_comp = std::make_unique<Minion>();
        minion_comp->target_spawnpoint = 1 - spawn_spawnpoint_index_;  // Target opposite spawnpoint
        minion_comp->has_reached_destination = false;
        minion_comp->is_dead = false;
        minion.add_component(std::move(minion_comp));
        
        LOG_INFO("Spawned minion at spawnpoint %d (pos: %.1f, %.1f, %.1f), targeting spawnpoint %d",
                 spawn_spawnpoint_index_,
                 spawn_pos.x, spawn_pos.y, spawn_pos.z,
                 1 - spawn_spawnpoint_index_);
        
        // Alternate spawn spawnpoint for next spawn
        spawn_spawnpoint_index_ = 1 - spawn_spawnpoint_index_;
    }
}

int MinionSpawnerSystem::get_minion_count() const {
    // This is a rough estimate; actual count would need to query entity manager
    return 0;
}
