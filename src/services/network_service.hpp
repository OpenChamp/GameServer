#pragma once

#include <map>
#include <string>
#include <functional>

#include <enet.h>
#include <systems/packet_validator.hpp>
#include "components/errors.hpp"

/**
 * Manages low level networking so that consumers can simply exchange packets
 */
class NetworkService {
public:
    NetworkService();
    NetworkService(int port, int max_clients);
    ~NetworkService();

    /**
     * Start networking as a server
     * @return An error code if something goes wrong, ERROR_CODE::ERROR_NONE otherwise
     */
    ERROR_CODE start_server();

    /**
     * @return True if this NetworkService is currently hosting a server
     */
    bool is_connected();

    /**
     * Stop hosting the network server
     */
    void disconnect();

    /**
     * Send a packet of specified type to a peer.
     * @param packet_type Type of packet to send
     * @param client_id Id of the peer to send the packet to
     */
    void send_packet(PACKET_TYPE packet_type, std::string client_id);

    /**
     * Send a packet to a peer.
     * @param packet Packet to send
     * @param client_id Id of the peer to send the packet to
     */
    void send_packet(ENetPacket* packet, std::string client_id);

    /**
     * Send a packet to all connected peers.
     * @param packet The packet to broadcast
     */
    void broadcast_packet(ENetPacket* packet);

    /**
     * Poll connections and run callbacks
     * @return True if no errors occured
     */
    bool run_callbacks();
    
    /**
     * Is fired when a new client connects to the enet server.
     * These clients will not be validated or authorized by NetworkService.
     * @param string ID of the client that has connected
     */
    std::function<void(std::string)> on_client_connected = nullptr;

    /**
     * Is fired when a client disconnects from the enet server
     * @param string ID of the client that has disconnected
     */
    std::function<void(std::string)> on_client_disconnected = nullptr;

    /**
     * Is fired when a client sends a packet to the server
     * @param string ID of the client that has sent the packet
     * @param packet Packet that was received
     */
    std::function<void(std::string, ENetPacket*)> on_packet_received = nullptr;
private:
    std::map<std::string, ENetPeer*> clients_;
    
    bool is_connected_ = false;
    int port_;
    int max_clients_;
    ENetHost* enet_server_;
};