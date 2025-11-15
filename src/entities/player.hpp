#pragma once

#include <string>
#include <chrono>

/**
 * Represents a connected player in the game.
 * Contains player state and metadata.
 * TODO: Refactor with more player-related components instead of a monolithic struct - cmkrist 15/11/2025
 */
struct Player {
    // Identification
    std::string client_id;           // Unique client identifier
    uint32_t player_id;              // Unique player ID (assigned by server)
    
    // Readiness state
    bool is_ready = false;
    
    // Connection metadata
    std::chrono::steady_clock::time_point last_activity;
    unsigned int latency_ms = 0;     // Last measured ping in milliseconds
    
    /**
     * Constructor - initializes player with client_id and timestamp.
     * @param id Client identifier
     */
    Player(const std::string& id) 
        : client_id(id), last_activity(std::chrono::steady_clock::now()) {}
    
    /**
     * Check if player connection is stale (no activity for timeout_ms).
     * @param timeout_ms Timeout in milliseconds
     * @return true if last activity exceeds timeout
     */
    bool is_stale(unsigned int timeout_ms = 30000) const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity);
        return elapsed.count() > timeout_ms;
    }
    
    /**
     * Update last activity timestamp to current time.
     */
    void update_activity() {
        last_activity = std::chrono::steady_clock::now();
    }
};
