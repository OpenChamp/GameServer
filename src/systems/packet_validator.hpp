#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Packet type enumeration for network communication.
 */
enum class PACKET_TYPE : uint8_t {
    PLAYER_READY,
    GAME_START,
    GAME_STATE,
    SPAWN_MAP,
    SPAWN_ENTITY,
    PLAYER_DISCONNECT,
    HEARTBEAT_PING,
    HEARTBEAT_PONG,
    MINION_STATE,  // Contains all active minion positions and states
};

/**
 * Validates and parses network packets.
 * Provides safety checks for all incoming network data.
 */
class PacketValidator {
public:
    /**
     * Get minimum packet size for a given packet type.
     * @param type Packet type to check
     * @return Minimum required packet size in bytes
     */
    static size_t get_min_size(PACKET_TYPE type) {
        switch (type) {
            case PACKET_TYPE::PLAYER_READY:
                return 2;  // type (1) + ready_status (1)
            case PACKET_TYPE::HEARTBEAT_PING:
            case PACKET_TYPE::HEARTBEAT_PONG:
                return 1;  // type only
            case PACKET_TYPE::MINION_STATE:
                return 5;  // type (1) + minion_count (4)
            case PACKET_TYPE::GAME_START:
            case PACKET_TYPE::GAME_STATE:
            case PACKET_TYPE::PLAYER_DISCONNECT:
            default:
                return 1;  // At minimum, type byte
        }
    }
    
    /**
     * Validate a received packet.
     * Checks length and type constraints.
     * @param packet_data Pointer to packet data
     * @param packet_length Length of packet data
     * @return true if packet is valid, false otherwise
     */
    static bool validate_packet(const uint8_t* packet_data, size_t packet_length) {
        // Minimum packet must contain type byte
        if (!packet_data || packet_length < 1) {
            return false;
        }
        
        // Parse packet type
        PACKET_TYPE type = (PACKET_TYPE)(packet_data[0]);
        
        // Check minimum size for this packet type
        size_t min_size = get_min_size(type);
        if (packet_length < min_size) {
            return false;
        }
        
        return true;
    }
    
    /**
     * Extract and validate player ready status from packet.
     * @param packet_data Pointer to packet data
     * @param packet_length Length of packet data
     * @param out_ready Output parameter for ready status
     * @return true if extraction successful
     */
    static bool extract_ready_status(const uint8_t* packet_data, size_t packet_length, bool& out_ready) {
        if (!validate_packet(packet_data, packet_length)) {
            return false;
        }
        
        if ((PACKET_TYPE)(packet_data[0]) != PACKET_TYPE::PLAYER_READY) {
            return false;
        }
        
        if (packet_length < 2) {
            return false;
        }
        
        out_ready = (packet_data[1] != 0);
        return true;
    }
};
