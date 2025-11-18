#pragma once

#include "system_context.hpp"
#include "input_system.hpp"
#include "collision_system.hpp"
#include "wave_system.hpp"
#include "movement_system.hpp"
#include "network_sync_system.hpp"
#include "combat_system.hpp"

/**
 * GameplayCoordinator - Orchestrates all game systems
 * 
 * RESPONSIBILITIES:
 *   - Update all systems in correct order
 *   - Maintain game logic loop
 *   - Ensure systems execute in dependency order
 * 
 * DOES NOT:
 *   - Manage players (use PlayerManager)
 *   - Handle networking directly (use NetworkService)
 *   - Manage game state transitions (use GameServer)
 * 
 * SYSTEM UPDATE ORDER (important for correctness):
 *   1. WaveSystem - Spawns new entities for this frame
 *   2. MovementSystem - Updates entity positions based on paths
 *   3. CollisionSystem - Resolves overlaps and pushes entities apart
 *   4. CombatSystem - Resolves damage and effects
 *   5. NetworkSyncSystem - Sends state updates to clients
 * 
 * USAGE:
 *   GameplayCoordinator gameplay;
 *   SystemContext ctx{...};
 *   gameplay.update(ctx);
 */
class GameplayCoordinator {
public:
    GameplayCoordinator();
    ~GameplayCoordinator() = default;
    
    // Prevent copying
    GameplayCoordinator(const GameplayCoordinator&) = delete;
    GameplayCoordinator& operator=(const GameplayCoordinator&) = delete;
    
    // Allow moving
    GameplayCoordinator(GameplayCoordinator&&) = default;
    GameplayCoordinator& operator=(GameplayCoordinator&&) = default;
    
    /**
     * Initialize the WaveSystem with required services.
     * Must be called before update() to properly set up entity spawning.
     * @param entity_manager Pointer to entity manager
     * @param network_service Pointer to network service (for broadcasting)
     * @param navigation_service Pointer to navigation service (for pathfinding)
     * @param map Pointer to map data
     */
    void initialize_wave_system(EntityManager* entity_manager, NetworkService* network_service, 
                               NavigationService* navigation_service, const Map* map);
    
    /**
     * Update all game systems for a single frame.
     * Systems are executed in dependency order to ensure correct behavior.
     * 
     * Order:
     *   1. WaveSystem::update() - Spawn new minions
     *   2. MovementSystem::update() - Move entities
     *   3. CombatSystem::update() - Auto-attacks and damage
     *   4. NetworkSyncSystem::update() - Broadcast state
     * 
     * @param ctx System context containing entity manager and services
     */
    void update(const SystemContext& ctx);
    
    /**
     * Get reference to input system.
     * @return Reference to InputSystem
     */
    InputSystem& get_input_system() { return input_system_; }
    
    /**
     * Get reference to wave system (for direct initialization if needed).
     * @return Reference to WaveSystem
     */
    WaveSystem& get_wave_system() { return *wave_system_; }
    
    /**
     * Get reference to collision system.
     * @return Reference to CollisionSystem
     */
    CollisionSystem& get_collision_system() { return collision_system_; }
    
    /**
     * Get reference to movement system.
     * @return Reference to MovementSystem
     */
    MovementSystem& get_movement_system() { return movement_system_; }
    
    /**
     * Get reference to network sync system.
     * @return Reference to NetworkSyncSystem
     */
    NetworkSyncSystem& get_network_sync_system() { return network_sync_system_; }
    
    /**
     * Get reference to combat system.
     * @return Reference to CombatSystem
     */
    CombatSystem& get_combat_system() { return combat_system_; }

private:
    InputSystem input_system_;
    std::unique_ptr<WaveSystem> wave_system_;
    CollisionSystem collision_system_;
    MovementSystem movement_system_;
    NetworkSyncSystem network_sync_system_;
    CombatSystem combat_system_;
};
