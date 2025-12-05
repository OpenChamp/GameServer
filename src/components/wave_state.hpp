#pragma once

#include "component.hpp"
#include <vector>
#include <string>
#include <cstdint>

/**
 * WaveStateComponent - Tracks wave progression state
 * 
 * USAGE: Add to a "wave entity" that represents the current wave
 * SYSTEMS: WaveSystem
 * 
 * The WaveSystem creates one of these entities to manage wave state.
 * Much cleaner than tracking multiple variables across the system.
 * 
 * RESPONSIBILITIES:
 *   - Track which phase the wave is in (waiting, spawning, active, complete)
 *   - Count minions spawned vs remaining
 *   - Store minion composition for this wave
 *   - Track elapsed time within each phase
 * 
 * BENEFITS:
 *   - Wave state is now queryable as a component
 *   - Can subscribe to wave changes via entity system
 *   - Clean separation of data from logic
 */
struct WaveStateComponent : public Component {
    /**
     * Wave phases - represents the lifecycle of a single wave
     */
    enum class WavePhase {
        WAITING,        // Waiting for timer before spawn (initial delay between waves)
        SPAWNING,       // Currently spawning minions (with delay between each minion)
        ACTIVE,         // Minions spawned, wave in progress (minions are alive/fighting)
        COMPLETE        // Wave finished, waiting for next (all minions killed or despawned)
    };
    
    // === Wave Identity ===
    int wave_number = 0;                            // Which wave (0-indexed)
    int wave_type = 0;                              // Special vs default (0=default, 1=special)
    
    // === Phase Tracking ===
    WavePhase phase = WavePhase::WAITING;           // Current phase
    float phase_elapsed_ms = 0.0f;                  // Time spent in current phase
    
    // === Minion Counting ===
    int minions_in_wave = 0;                        // Total minions for this wave
    int minions_spawned = 0;                        // How many have been spawned so far
    int minions_remaining = 0;                      // How many are still alive
    
    // === Composition ===
    std::vector<std::string> minion_composition;    // List of minion templates for this wave
    
    // === Timing ===
    static constexpr float WAVE_START_DELAY_MS = 3000.0f;  // 3 seconds before first minion
    static constexpr float MINION_SPAWN_DELAY_MS = 400.0f;  // 400ms between minions
    static constexpr float WAVE_COMPLETE_DELAY_MS = 2000.0f; // 2 seconds after last minion killed
    
    COMPONENT_TYPE_ID(WaveStateComponent, 2050)
};
