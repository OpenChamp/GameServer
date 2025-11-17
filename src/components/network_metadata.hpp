#pragma once

#include "component.hpp"
#include <chrono>

/**
 * NetworkMetadataComponent - Player connection metadata
 * 
 * USAGE: Track connection health and latency for player entities
 * SYSTEMS: PlayerManager, NetworkService
 * 
 * Replaces Player struct fields: last_activity, latency_ms
 * Provides methods to check connection staleness and update activity.
 */
struct NetworkMetadataComponent : public Component {
    std::chrono::steady_clock::time_point last_activity;
    unsigned int latency_ms = 0;    // Last measured ping in milliseconds
    
    /**
     * Constructor - initializes last_activity to current time
     */
    NetworkMetadataComponent() 
        : last_activity(std::chrono::steady_clock::now()) {}
    
    /**
     * Check if player connection is stale (no activity for timeout_ms).
     * @param timeout_ms Timeout in milliseconds (default: 30 seconds)
     * @return true if last activity exceeds timeout
     */
    bool is_stale(unsigned int timeout_ms = 30000) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_activity);
        return elapsed.count() > timeout_ms;
    }
    
    /**
     * Update last activity timestamp to current time.
     * Call this whenever the player sends a packet.
     */
    void update_activity() {
        last_activity = std::chrono::steady_clock::now();
    }
    
    COMPONENT_TYPE_ID(NetworkMetadataComponent, 3003)
};
