#include "gameserver.hpp"
#include "packet_validator.hpp"
#include <log.hpp>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <chrono>

GameServer::GameServer(int port, int max_clients)
    : port_(port)
    , max_clients_(max_clients)
    , enet_server_(nullptr)
    , shutdown_requested_(false)
    , current_state_(GAME_STATE::PREGAME)
    , last_minion_broadcast_(std::chrono::high_resolution_clock::now()) {
}

GameServer::~GameServer() {
    // Clean up ENet resources
    if (enet_server_) {
        // Clean up all peer data
        if (enet_server_->peers) {
            for (size_t i = 0; i < (size_t)enet_server_->connectedPeers; ++i) {
                ENetPeer* peer = &enet_server_->peers[i];
                if (peer && peer->data != nullptr) {
                    delete (std::string*)(peer->data);
                    peer->data = nullptr;
                }
            }
        }
        
        enet_host_destroy(enet_server_);
        enet_server_ = nullptr;
    }
    
    enet_deinitialize();
    LOG_INFO("GameServer destroyed");
}

ERROR_CODE GameServer::initialize() {
    LOG_INFO("Initializing GameServer on port %d with max %d clients", port_, max_clients_);
    
    // Initialize ENet
    int enet_init_result = enet_initialize();
    LOG_INFO("enet_initialize() returned: %d", enet_init_result);
    
    if (enet_init_result != 0) {
        LOG_ERROR("Failed to initialize ENet");
        return ERROR_CODE::ERROR_ENET_INIT_FAILED;
    }
    
    ENetAddress address;
    memset(&address, 0, sizeof(ENetAddress));
    address.host = ENET_HOST_ANY;
    address.port = (enet_uint16)port_;
    address.sin6_scope_id = 0;
    
    LOG_INFO("Attempting to bind to port %d with max_clients=%zu", port_, (size_t)max_clients_);
    
    // Create server host
    enet_server_ = enet_host_create(&address, (size_t)max_clients_, 2, 0, 0);
    if (!enet_server_) {
        LOG_ERROR("Failed to create ENet server host on port %d - port may already be in use or permission denied", port_);
        enet_deinitialize();
        return ERROR_CODE::ERROR_ENET_CREATION_FAILED;
    }
    
    LOG_INFO("Server initialized successfully on port %d", port_);
    
    // Initialize ECS systems
    map_entity_ = MapSystem::load_default_map(entity_manager_);
    if (!map_entity_) {
        LOG_ERROR("Failed to load map");
        return ERROR_CODE::ERROR_ENET_CREATION_FAILED;
    }
    minion_spawner_ = std::make_unique<MinionSpawnerSystem>();
    minion_movement_ = std::make_unique<MinionMovementSystem>();
    minion_damage_ = std::make_unique<MinionDamageSystem>();
    
    return ERROR_CODE::ERROR_NONE;
}

bool GameServer::service_network(unsigned int timeout_ms) {
    if (!enet_server_) {
        return false;
    }
    
    ENetEvent event;
    int service_result = enet_host_service(enet_server_, &event, timeout_ms);
    
    if (service_result < 0) {
        LOG_ERROR("ENet service error occurred");
        return false;
    }
    
    if (service_result == 0) {
        // Timeout - no events
        return false;
    }
    
    // Process the event
    switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            on_client_connect(event);
            break;
            
        case ENET_EVENT_TYPE_RECEIVE: {
            if (event.peer->data == nullptr) {
                LOG_WARN("Received packet from unknown client, channel %u, length %u",
                        event.channelID, (unsigned int)event.packet->dataLength);
            } else {
                std::string* client_id_ptr = (std::string*)(event.peer->data);
                LOG_DEBUG("Received packet from %s, channel %u, length %u",
                         client_id_ptr->c_str(), event.channelID, (unsigned int)event.packet->dataLength);
                
                // Validate and process packet
                if (PacketValidator::validate_packet(event.packet->data, event.packet->dataLength)) {
                    PACKET_TYPE packet_type = (PACKET_TYPE)(event.packet->data[0]);
                    
                    switch (packet_type) {
                        case PACKET_TYPE::PLAYER_READY:
                            handle_player_ready_packet(event, event.packet->data, event.packet->dataLength);
                            break;
                            
                        default:
                            LOG_WARN("Unknown packet type: %d", (int)(packet_type));
                            break;
                    }
                } else {
                    LOG_WARN("Invalid packet received from %s", client_id_ptr->c_str());
                }
                
                // Update last activity for this player
                auto it = players_.find(*client_id_ptr);
                if (it != players_.end()) {
                    it->second.update_activity();
                }
            }
            
            enet_packet_destroy(event.packet);
            break;
        }
            
        case ENET_EVENT_TYPE_DISCONNECT:
            on_client_disconnect(event);
            break;
            
        default:
            break;
    }
    
    return true;
}

void GameServer::send_packet(PACKET_TYPE packet_type, ENetPeer* peer) {
    uint8_t packet_data[1];
    packet_data[0] = static_cast<uint8_t>(packet_type);
    
    ENetPacket* packet = enet_packet_create(packet_data, sizeof(packet_data), ENET_PACKET_FLAG_RELIABLE);
    if (packet) {
        enet_peer_send(peer, 0, packet);
        LOG_DEBUG("Sent packet type %d to peer", (int)packet_type);
    } else {
        LOG_ERROR("Failed to create packet for type %d", (int)packet_type);
    }
}

void GameServer::run() {
    LOG_INFO("Starting server main loop");
    
    while (!shutdown_requested_) {
        if(!frame_timer_.is_frame()) {
            // Wait for the next tick!
            continue;
        }

        // Service network
        service_network(0);
        
        // Update ECS systems
        if (map_entity_) {
            // Update minion spawner
            minion_spawner_->update(entity_manager_, frame_timer_.frame_duration_in_ms(), map_entity_, current_state_);
            
            // Update minion movement
            minion_movement_->update(entity_manager_, frame_timer_.frame_duration_in_ms(), map_entity_);
            
            // Update minion damage and deaths
            auto dead_minions = minion_damage_->update(entity_manager_);
            
            // Remove dead minions from the entity manager
            for (EntityID dead_id : dead_minions) {
                entity_manager_.destroy_entity(dead_id);
            }
            
            // Broadcast minion states to all clients (rate-limited)
            broadcast_minion_states();
        }
    }
    
    LOG_INFO("Server main loop ended");
}

void GameServer::request_shutdown() {
    LOG_INFO("Shutdown requested");
    shutdown_requested_ = true;
}

bool GameServer::is_shutdown_requested() const {
    return shutdown_requested_;
}

bool GameServer::is_lobby_ready() const {
    if (players_.empty()) {
        return false;
    }
    
    for (const auto& pair : players_) {
        if (!pair.second.is_ready) {
            return false;
        }
    }
    
    return true;
}

GAME_STATE GameServer::get_current_state() const {
    return current_state_;
}

bool GameServer::try_transition_state(GAME_STATE new_state) {
    if (!is_valid_state_transition(current_state_, new_state)) {
        LOG_WARN("Invalid state transition from %s to %s",
                game_state_to_string(current_state_),
                game_state_to_string(new_state));
        return false;
    }
    
    GAME_STATE old_state = current_state_;
    current_state_ = new_state;
    LOG_INFO("State transition: %s -> %s",
            game_state_to_string(old_state),
            game_state_to_string(new_state));
    
    return true;
}

size_t GameServer::get_player_count() const {
    return players_.size();
}

int GameServer::get_max_clients() const {
    return max_clients_;
}

bool GameServer::is_lobby_full() const {
    return (int)players_.size() >= max_clients_;
}

void GameServer::on_client_connect(ENetEvent& event) {
    std::string client_id = std::string("client_") + std::to_string(event.peer->incomingSessionID);
    
    LOG_INFO("Client connected: %s (%x:%u)",
            client_id.c_str(), event.peer->address.host, event.peer->address.port);
    
    // Create player
    Player new_player(client_id);
    new_player.player_id = (uint32_t)players_.size();
    
    // Store in map
    auto result = players_.emplace(client_id, new_player);
    
    // Store client_id string pointer in ENet peer data
    event.peer->data = new std::string(client_id);
    
    // Tell the player which map to load
    if (map_entity_) {
        ENetPacket* map_packet = MapSystem::serialize_map(map_entity_);
        if (map_packet) {
            enet_peer_send(event.peer, 0, map_packet);
            LOG_INFO("Sent map data to client %s", client_id.c_str());
        } else {
            LOG_WARN("Failed to serialize map data for client %s", client_id.c_str());
        }
    }

    LOG_INFO("Player %s added. Total players: %zu/%d",
            client_id.c_str(), players_.size(), max_clients_);
    
    if (is_lobby_full()) {
        LOG_INFO("Lobby full! Waiting for all players to be ready (%zu/%d)",
                players_.size(), max_clients_);
    } else {
        LOG_INFO("Waiting for more players... (%zu/%d)",
                players_.size(), max_clients_);
    }
    
    broadcast_player_list();
}

bool GameServer::handle_player_ready_packet(ENetEvent& event, const uint8_t* packet_data, size_t packet_length) {
    if (!event.peer->data) {
        LOG_WARN("Ready packet from peer with no data");
        return false;
    }
    
    bool is_ready = false;
    if (!PacketValidator::extract_ready_status(packet_data, packet_length, is_ready)) {
        LOG_WARN("Failed to extract ready status from packet");
        return false;
    }
    
    std::string* client_id_ptr = (std::string*)(event.peer->data);
    const std::string& client_id = *client_id_ptr;
    
    // Update player ready status
    auto it = players_.find(client_id);
    if (it == players_.end()) {
        LOG_WARN("Received ready packet from unknown player: %s", client_id.c_str());
        return false;
    }
    
    it->second.is_ready = is_ready;
    LOG_INFO("Player %s is now %s", client_id.c_str(), is_ready ? "ready" : "not ready");
    
    // Check if we should transition to ONGOING
    if (current_state_ == GAME_STATE::PREGAME && 
        is_lobby_full() && 
        is_lobby_ready()) {
        
        LOG_INFO("All players ready! Transitioning to ONGOING state");
        try_transition_state(GAME_STATE::ONGOING);
    }
    
    broadcast_player_list();
    return true;
}

void GameServer::on_client_disconnect(ENetEvent& event) {
    if (!event.peer || !event.peer->data) {
        LOG_WARN("Disconnect from peer with no data");
        return;
    }
    
    std::string* client_id_ptr = (std::string*)(event.peer->data);
    const std::string& client_id = *client_id_ptr;
    
    LOG_INFO("Client disconnected: %s", client_id.c_str());
    
    // Remove player from map
    auto it = players_.find(client_id);
    if (it != players_.end()) {
        players_.erase(it);
        LOG_INFO("Player removed. Remaining players: %zu", players_.size());
    }
    
    // Clean up peer data
    if (client_id_ptr) {
        delete client_id_ptr;
        event.peer->data = nullptr;
    }
    
    // Handle state changes if game was ongoing
    if (current_state_ == GAME_STATE::ONGOING && players_.empty()) {
        LOG_WARN("All players disconnected. Returning to PREGAME");
        try_transition_state(GAME_STATE::PREGAME);
    }
    
    broadcast_player_list();
}

void GameServer::broadcast_player_list() {
    // TODO: Implement broadcasting player list to all connected clients
    // For now, this is a placeholder for future networking implementation
}

void GameServer::broadcast_minion_states() {
    // Check if enough time has passed since last broadcast (rate limiting)
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<float>(now - last_minion_broadcast_);
    if (elapsed.count() < MINION_BROADCAST_INTERVAL) {
        return;  // Not enough time has passed
    }
    
    // Update last broadcast time
    last_minion_broadcast_ = now;
    
    // Only broadcast during ONGOING state
    if (current_state_ != GAME_STATE::ONGOING) {
        return;
    }
    
    // Serialize minion states
    ENetPacket* packet = MinionSerializer::serialize_minions(entity_manager_);
    if (!packet) {
        // No minions to broadcast yet
        return;
    }
    
    // Broadcast to all connected peers
    if (enet_server_) {
        enet_host_broadcast(enet_server_, 0, packet);
        LOG_DEBUG("Broadcasted minion state packet to all clients");
    } else {
        enet_packet_destroy(packet);
    }
}

bool GameServer::is_valid_state_transition(GAME_STATE from, GAME_STATE to) const {
    if (from == to) {
        return true;  // Stay in same state is always valid
    }
    
    switch (from) {
        case GAME_STATE::PREGAME:
            return to == GAME_STATE::ONGOING;
            
        case GAME_STATE::ONGOING:
            return to == GAME_STATE::PAUSED || to == GAME_STATE::ENDING;
            
        case GAME_STATE::PAUSED:
            return to == GAME_STATE::ONGOING || to == GAME_STATE::ENDING;
            
        case GAME_STATE::ENDING:
            return to == GAME_STATE::PREGAME;
            
        default:
            return false;
    }
}
