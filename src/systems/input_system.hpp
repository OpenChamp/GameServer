#pragma once

#include "system_context.hpp"
#include "math.hpp"
#include <queue>
#include <cstdint>

struct Entity;
typedef uint32_t EntityID;

/**
 * InputSystem - Processes player input and translates it into entity actions
 * 
 * RESPONSIBILITIES:
 *   - Queue and process movement input from players
 *   - Translate input into entity state changes and target positions
 *   - Validate input against entity and game state
 *   - Coordinate with MovementSystem for execution
 * 
 * ARCHITECTURE:
 *   PacketHandler -> InputSystem -> MovementSystem
 *   
 *   - PacketHandler: Validates network packets
 *   - InputSystem: Processes input logic and queues actions
 *   - MovementSystem: Executes movement and pathfinding
 * 
 * USAGE:
 *   InputSystem input_system;
 *   // When packet arrives:
 *   input_system.queue_movement_input(entity_id, target_position);
 *   // In game loop:
 *   input_system.update(ctx);
 */
class InputSystem {
public:
    InputSystem() = default;
    ~InputSystem() = default;
    
    // Prevent copying
    InputSystem(const InputSystem&) = delete;
    InputSystem& operator=(const InputSystem&) = delete;
    
    // Allow moving
    InputSystem(InputSystem&&) = default;
    InputSystem& operator=(InputSystem&&) = default;
    
    /**
     * Queue a movement input for an entity.
     * Called by PacketHandler when receiving PLAYER_MOVE packets.
     * 
     * @param entity_id ID of the entity to move
     * @param target_position Target position to move to
     */
    void queue_movement_input(EntityID entity_id, const Vec2& target_position);
    
    /**
     * Process all queued inputs and apply them to entities.
     * Called once per game frame by GameplayCoordinator.
     * 
     * @param ctx System context containing entity manager
     */
    void update(const SystemContext& ctx);
    
private:
    /**
     * Movement input request
     */
    struct MovementInput {
        EntityID entity_id;
        Vec2 target_position;
    };
    
    // Queue of pending movement inputs
    std::queue<MovementInput> movement_inputs_;
    
    /**
     * Process a single movement input request.
     * Uses pathfinding (A*) to find a navmesh-aware path to avoid holes.
     * Falls back to direct movement if pathfinding fails.
     * 
     * @param entity Entity to move
     * @param target_position Target position
     * @param ctx System context with NavigationService and Map
     * @return true if input was processed successfully
     */
    static bool process_movement_input(Entity& entity, const Vec2& target_position, const SystemContext& ctx);
};
