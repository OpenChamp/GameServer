#pragma once

/**
 * Game state enumeration representing the server's operational state.
 * 
 * Transitions:
 *   PREGAME -> ONGOING (all players ready, lobby full)
 *   ONGOING -> PAUSED (admin command or error)
 *   PAUSED -> ONGOING (resume)
 *   PAUSED -> ENDING (admin command)
 *   ONGOING -> ENDING (game finished)
 *   ENDING -> PREGAME (reset for new game)
 */
enum class GAME_STATE {
    PREGAME,
    ONGOING,
    PAUSED,
    ENDING,
};

/**
 * Convert GAME_STATE enum to human-readable string.
 * @param state Game state to convert
 * @return String representation of the state
 */
inline const char* game_state_to_string(GAME_STATE state) {
    switch (state) {
        case GAME_STATE::PREGAME: return "PREGAME";
        case GAME_STATE::ONGOING: return "ONGOING";
        case GAME_STATE::PAUSED: return "PAUSED";
        case GAME_STATE::ENDING: return "ENDING";
        default: return "UNKNOWN";
    }
}
