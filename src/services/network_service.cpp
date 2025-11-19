#include "network_service.hpp"

#include "systems/math.hpp"
#include "libs/log.hpp"

struct NetworkService::NetworkBackend {
    NetworkBackend() {

    }

    std::map<std::string, ENetPeer*> clients_;
    ENetHost* enet_server_;
};

NetworkService::NetworkService() {
    port_ = -1;
    max_clients_ = 0;
}

NetworkService::NetworkService(int port, int max_clients) {
    port_ = port;
    max_clients_ = max_clients;
}

NetworkService::~NetworkService() {
    if(is_connected()) {
        disconnect();
    }

    enet_deinitialize();
}

ERROR_CODE NetworkService::start_server() {
    LOG_INFO("Initializing GameServer on port %d with max %d clients", port_, max_clients_);
    // Initialize ENet
    int enet_init_result = enet_initialize();    
    if (enet_init_result != 0) {
        LOG_ERROR("Failed to initialize ENet");
        return ERROR_CODE::ERROR_ENET_INIT_FAILED;
    }

    backend_ = std::make_unique<NetworkService::NetworkBackend>();
    
    ENetAddress address;
    memset(&address, 0, sizeof(ENetAddress));
    address.host = ENET_HOST_ANY;
    address.port = (enet_uint16)port_;
    address.sin6_scope_id = 0;
    
    LOG_INFO("Attempting to bind to port %d with max_clients=%zu", port_, (size_t)max_clients_);
    
    // Create server host
    backend_->enet_server_ = enet_host_create(&address, (size_t)max_clients_, 2, 0, 0);
    if (!backend_->enet_server_) {
        LOG_ERROR("Failed to create ENet server host on port %d - port may already be in use or permission denied", port_);
        enet_deinitialize();
        return ERROR_CODE::ERROR_ENET_CREATION_FAILED;
    }
    
    is_connected_ = true;
    LOG_INFO("Server initialized successfully on port %d", port_);
    return ERROR_CODE::ERROR_NONE;
}

void NetworkService::disconnect() {
    // Clean up ENet resources
    if (backend_->enet_server_) {
        // Clean up all peer data
        if (backend_->enet_server_->peers) {
            for (size_t i = 0; i < (size_t)backend_->enet_server_->connectedPeers; ++i) {
                ENetPeer* peer = &backend_->enet_server_->peers[i];
                if (peer && peer->data != nullptr) {
                    delete (std::string*)(peer->data);
                    peer->data = nullptr;
                }
            }
        }
        
        enet_host_destroy(backend_->enet_server_);
        backend_->enet_server_ = nullptr;
    }
    is_connected_ = false;
}

void NetworkService::disconnect_client(const std::string& client_id) {
    auto it = backend_->clients_.find(client_id);
    if (it != backend_->clients_.end()) {
        ENetPeer* peer = it->second;
        if (peer) {
            // Request graceful disconnection
            enet_peer_disconnect(peer, 0);
        }
        // Remove from clients map
        backend_->clients_.erase(it);
    }
}

bool NetworkService::run_callbacks() {
    if (!backend_->enet_server_) {
        return false;
    }
    
    ENetEvent event;
    int service_result = enet_host_service(backend_->enet_server_, &event, 0);
    
    if (service_result < 0) {
        LOG_ERROR("ENet service error occurred");
        return false;
    }
    
    if (service_result == 0) {
        // Timeout - no events
        return false;
    }
    
    std::string client_id;
    std::string* client_id_ptr;
    // Process the event
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            client_id = std::string("client_") + std::to_string(event.peer->incomingSessionID);
            
            LOG_INFO("Client connected: %s (%x:%u)",
                    client_id.c_str(), event.peer->address.host, event.peer->address.port);
            
            // Store client_id string pointer in ENet peer data
            event.peer->data = new std::string(client_id);
            backend_->clients_.emplace(client_id, event.peer);
            
            if(on_client_connected) {
                on_client_connected(client_id);
            }
            break;
            
        case ENET_EVENT_TYPE_RECEIVE: {
            if (event.peer->data == nullptr) {
                LOG_WARN("Received packet from unknown client, channel %u, length %u",
                        event.channelID, (unsigned int)event.packet->dataLength);
                break;
            }

            client_id_ptr = (std::string*)(event.peer->data);
            LOG_DEBUG("Received packet from %s, channel %u, length %u",
                        client_id_ptr->c_str(), event.channelID, (unsigned int)event.packet->dataLength);
                
            if(on_packet_received) {
                on_packet_received(*client_id_ptr, event.packet->data, event.packet->dataLength);
            }

            enet_packet_destroy(event.packet);
            break;
        }
            
        case ENET_EVENT_TYPE_DISCONNECT:
            if (!event.peer || !event.peer->data) {
                LOG_WARN("Disconnect from peer with no data");
                break;
            }
            
            client_id_ptr = (std::string*)(event.peer->data);
            client_id = *client_id_ptr;
            
            // Clean up peer data
            if (client_id_ptr) {
                delete client_id_ptr;
                event.peer->data = nullptr;
            }

            backend_->clients_.erase(client_id);

            LOG_INFO("Client disconnected: %s", client_id.c_str());

            if(on_client_disconnected) {
                on_client_disconnected(client_id);
            }
            break;
            
        default:
            break;
    }
    
    return true;
}

void NetworkService::send_packet(PACKET_TYPE packet_type, std::string client_id) {
    auto client_it = backend_->clients_.find(client_id);
    if(client_it == backend_->clients_.end()) {
        LOG_ERROR("Failed to send packet to client %s, client is invalid", client_id.c_str());
        return;
    }

    ENetPeer* peer = client_it->second;
    uint8_t packet_data[1];
    packet_data[0] = static_cast<uint8_t>(packet_type);
    
    ENetPacket* packet = enet_packet_create(packet_data, sizeof(packet_data), ENET_PACKET_FLAG_RELIABLE);
    if (packet) {
        int err = enet_peer_send(peer, 0, packet);
        if (err < 0) {
            LOG_ERROR("Failed to send packet type %d to peer", (int)packet_type);
        } else {
            LOG_DEBUG("Sent packet type %d to peer", (int)packet_type);
        }
    } else {
        LOG_ERROR("Failed to create packet for type %d", (int)packet_type);
    }
}

void NetworkService::send_packet(PACKET_TYPE packet_type, const std::string& data, const std::string& client_id) {
    auto client_it = backend_->clients_.find(client_id);
    if(client_it == backend_->clients_.end()) {
        LOG_ERROR("Failed to send packet to client %s, client is invalid", client_id.c_str());
        return;
    }
    // Verify data size fits in uint16_t
    if (data.size() > uint16_t(-1)) {
        LOG_ERROR("Data size %zu exceeds maximum allowed size for packet", data.size());
        return;
    }
    ENetPeer* peer = client_it->second;
    std::vector<uint8_t> packet_data;
    packet_data.push_back(static_cast<uint8_t>(packet_type));
    // 2 bytes for length (Big-endian)
    uint16_t data_length = static_cast<uint16_t>(data.size());
    packet_data.push_back((data_length >> 8) & 0xFF);
    // Add data
    packet_data.push_back(data_length & 0xFF);
    packet_data.insert(packet_data.end(), data.begin(), data.end());

    ENetPacket* packet = enet_packet_create(packet_data.data(), packet_data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (packet) {
        int err = enet_peer_send(peer, 0, packet);
        if (err < 0) {
            LOG_ERROR("Failed to send packet type %d to peer", (int)packet_type);
        } else {
            LOG_DEBUG("Sent packet type %d to peer", (int)packet_type);
        }
    } else {
        LOG_ERROR("Failed to create packet for type %d", (int)packet_type);
    }
}

void NetworkService::send_packet(const std::vector<uint8_t>& data, std::string client_id) {
    auto peer_it = backend_->clients_.find(client_id);

    if(peer_it == backend_->clients_.end()) {
        LOG_ERROR("Failed to send packet to client %s, client is invalid", client_id.c_str());
        return;
    }
    
    // Create packet
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        LOG_ERROR("Failed to send packet to client %s, failed to allocate packet");
        return;
    }

    int err = enet_peer_send(peer_it->second, 0, packet);
    if (err < 0) {
        LOG_ERROR("Failed to send packet to client %s", client_id.c_str());
    } else {
        LOG_DEBUG("Sent packet to client %s", client_id.c_str());
    }
    enet_packet_destroy(packet);
}



void NetworkService::broadcast_packet(const std::vector<uint8_t>& data) {
    // Create packet
    ENetPacket* packet = enet_packet_create(data.data(), data.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        LOG_ERROR("Failed to send packet to client %s, failed to allocate packet");
        return;
    }

    // Broadcast to all connected peers
    if (backend_->enet_server_) {
        enet_host_broadcast(backend_->enet_server_, 0, packet);
        LOG_DEBUG("Broadcasted minion state packet to all clients");
    } else {
        // destroy packet if server not running
        enet_packet_destroy(packet);
    }
}

void NetworkService::broadcast_packet(const PACKET_TYPE& packet_type) {
    // Create packet
    uint8_t packet_data[1];
    packet_data[0] = static_cast<uint8_t>(packet_type);
    ENetPacket* packet = enet_packet_create(packet_data, sizeof(packet_data), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        LOG_ERROR("Failed to send packet, failed to allocate packet");
        return;
    }
    LOG_INFO("Broadcasting packet type %d to all clients", (int)packet_type);
    
    // Broadcast to all connected peers
    if (backend_->enet_server_) {
        enet_host_broadcast(backend_->enet_server_, 0, packet);
    } else {
        enet_packet_destroy(packet);
    }
}

bool NetworkService::is_connected() {
    return is_connected_;
}