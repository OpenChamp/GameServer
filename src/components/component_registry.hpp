#pragma once

#include <cstdint>

/**
 * Central registry of all component type IDs.
 * 
 * Organized by range:
 *  0-999:     Reserved (do not use)
 *  1000-1999: Physics, Transform, Movement
 *  2000-2999: Gameplay (Stats, Experience, Combat, AI)
 *  3000-3999: Network, Player-specific
 *  4000-4999: Client-side (visual, UI)
 *  5000+:     Custom, Modding
 */
namespace ComponentTypes {
    // === Core Gameplay (2000-2099) ===
    constexpr uint32_t MOVEMENT = 2001;
    constexpr uint32_t STATS = 2002;
    
    // === State Management (2010-2099) ===
    constexpr uint32_t ENTITY_STATE = 2010;
    constexpr uint32_t PATHFINDING = 2009;
    constexpr uint32_t EXPERIENCE = 2011;
    
    // === Network & Player (3000-3099) ===
    constexpr uint32_t CLIENT_INFO = 3001;
    constexpr uint32_t READINESS = 3002;
    constexpr uint32_t NETWORK_METADATA = 3003;
    constexpr uint32_t NETWORK_ENTITY = 3004;
    
    // === Wave System (2050-2099) ===
    constexpr uint32_t WAVE_STATE = 2050;
}