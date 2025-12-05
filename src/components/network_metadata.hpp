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
    std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();
    unsigned int latency_ms = 0;    // Last measured ping in milliseconds
    
    COMPONENT_TYPE_ID(NetworkMetadataComponent, 3003)
};
