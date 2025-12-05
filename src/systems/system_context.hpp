#pragma once

#include <memory>
#include "entity_manager.hpp"

// Forward declarations to avoid circular dependencies
class NavigationService;
class NetworkService;
class InputSystem;
struct Map;

/**
 * SystemContext - Unified context for all game systems
 * 
 * Contains references to the entity manager and all required services.
 * Passed to all system update methods instead of varying parameters.
 * 
 * BENEFITS:
 *   - Single, extensible context structure
 *   - No need to modify system signatures when adding new services
 *   - Clear documentation of system dependencies
 *   - Easier to mock for testing
 * 
 * USAGE:
 *   SystemContext ctx{
 *       .entity_manager = entity_manager_,
 *       .navigation_service = navigation_service_.get(),
 *       .network_service = &network_service_,
 *       .map = map_pointer_.get(),
 *       .delta_time_ms = delta_time_ms
 *   };
 *   
 *   movement_system_.update(ctx);
 *   wave_system_.update(ctx);
 *   network_sync_system_.update(ctx);
 */
struct SystemContext {
    // === ECS Core ===
    EntityManager& entity_manager;          // Required: access to all entities
    
    // === Optional Services ===
    InputSystem* input_system = nullptr;            // For processing player input
    NavigationService* navigation_service = nullptr;  // For pathfinding requests
    NetworkService* network_service = nullptr;        // For network broadcasting
    const Map* map = nullptr;                          // For map data queries
    
    // === Timing ===
    float delta_time_ms = 0.0f;             // Frame delta time in milliseconds
    
    // === Constructor ===
    SystemContext(EntityManager& em)
        : entity_manager(em) {}
    
    // === Convenience Methods ===
    
    /**
     * Check if navigation service is available.
     * @return true if navigation_service is not nullptr
     */
    bool has_navigation_service() const { 
        return navigation_service != nullptr; 
    }
    
    /**
     * Check if network service is available.
     * @return true if network_service is not nullptr
     */
    bool has_network_service() const { 
        return network_service != nullptr; 
    }
    
    /**
     * Check if map data is available.
     * @return true if map is not nullptr
     */
    bool has_map() const { 
        return map != nullptr; 
    }
};
