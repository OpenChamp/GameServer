#pragma once

#include <string>
#include <cstdint>

class EntityManager;
class PlayerManager;
class NetworkService;
class InputSystem;

/**
 * PacketHandler - Centralized packet processing
 * 
 * RESPONSIBILITIES:
 *   - Validate incoming packets
 *   - Dispatch to appropriate handlers
 *   - Update player state based on packets
 *   - Coordinate with managers
 * 
 * DOES NOT:
 *   - Manage player state directly (use PlayerManager)
 *   - Manage entities directly (use EntityManager)
 *   - Handle networking (use NetworkService)
 * 
 * USAGE:
 *   PacketHandler handler(&player_mgr, &entity_mgr, &net_service);
 *   handler.handle_packet(client_id, packet_data, packet_length);
 */
class PacketHandler {
public:
    /**
     * Constructor
     * @param player_manager Pointer to PlayerManager for player state updates
     * @param entity_manager Pointer to EntityManager for entity access
     * @param network_service Pointer to NetworkService for packet broadcasting
     * @param input_system Pointer to InputSystem for queueing player input
     */
    PacketHandler(PlayerManager* player_manager, 
                  EntityManager* entity_manager,
                  NetworkService* network_service,
                  InputSystem* input_system);
    
    /**
     * Process an incoming packet from a client.
     * Validates, dispatches, and handles the packet appropriately.
     * @param client_id Source client identifier
     * @param data Packet bytes
     * @param length Packet length in bytes
     */
    void handle_packet(const std::string& client_id, const uint8_t* data, size_t length);

private:
    PlayerManager* player_manager_;
    EntityManager* entity_manager_;
    NetworkService* network_service_;
    InputSystem* input_system_;
    
    /**
     * Handle PLAYER_READY packet.
     * Updates player readiness state and checks for game start condition.
     * @param client_id Source client identifier
     * @param data Packet bytes
     * @param length Packet length in bytes
     * @return true if handled successfully
     */
    bool handle_player_ready_packet(const std::string& client_id, const uint8_t* data, size_t length);
    
    /**
     * Handle PLAYER_MOVE packet.
     * Finds the Players entity and moves it towards the position supplied by the player.
     * @param client_id Source client identifier
     * @param data Packet bytes
     * @param length Packet length in bytes
     * @return true if handled successfully
     */
    bool handle_player_move_packet(const std::string& client_id, const uint8_t* data, size_t length);
    /**
     * Handle other packet types (extensible for future packets).
     * @param client_id Source client identifier
     * @param packet_type Type of packet received
     * @param data Packet bytes
     * @param length Packet length in bytes
     * @return true if handled successfully
     */
    bool handle_other_packets(const std::string& client_id, uint8_t packet_type, 
                            const uint8_t* data, size_t length);
};
