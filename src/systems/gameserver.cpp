#include "gameserver.hpp"
#include "packet_validator.hpp"
#include <log.hpp>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <chrono>

GameServer::GameServer(int port, int max_clients)
    : max_clients_(max_clients)
    , shutdown_requested_(false)
    , current_state_(GAME_STATE::PREGAME)
    , last_minion_broadcast_(std::chrono::high_resolution_clock::now()) {
    network_service_ = NetworkService(port, max_clients);
    network_service_.on_client_connected = [this](std::string client_id) { on_client_connect(client_id); };
    network_service_.on_client_disconnected = [this](std::string client_id) { on_client_disconnect(client_id); };
    network_service_.on_packet_received = [this](std::string client_id, ENetPacket* packet) { on_packet_received(client_id, packet); };
    network_service_.start_server();
}

GameServer::~GameServer() {
    if(network_service_.is_connected()) {
        network_service_.disconnect();
    }
    LOG_INFO("GameServer destroyed");
}

ERROR_CODE GameServer::initialize() {
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

void GameServer::run() {
    LOG_INFO("Starting server main loop");
    
    while (!shutdown_requested_) {
        if(!frame_timer_.is_frame()) {
            // Wait for the next tick!
            continue;
        }

        if(network_service_.is_connected()) {
            network_service_.run_callbacks();
        }
        
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

void GameServer::on_client_connect(std::string client_id) {
    // Create player
    Player new_player(client_id);
    new_player.player_id = (uint32_t)players_.size();
    
    // Store in map
    auto result = players_.emplace(client_id, new_player);
    
    // Tell the player which map to load
    if (map_entity_) {
        ENetPacket* map_packet = MapSystem::serialize_map(map_entity_);
        network_service_.send_packet(map_packet, client_id);
        LOG_INFO("Sent map data to client %s", client_id.c_str());
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

void GameServer::on_packet_received(std::string client_id, ENetPacket* packet) {
    // Validate and process packet
    if (!PacketValidator::validate_packet(packet->data, packet->dataLength)) {
        LOG_WARN("Invalid packet received from %s", client_id.c_str());
        return;
    }
    PACKET_TYPE packet_type = (PACKET_TYPE)(packet->data[0]);
    
    switch (packet_type) {
        case PACKET_TYPE::PLAYER_READY:
            handle_player_ready_packet(client_id, packet->data, packet->dataLength);
            break;
            
        default:
            LOG_WARN("Unknown packet type: %d", (int)(packet_type));
            break;
    }
        
    // Update last activity for this player
    auto it = players_.find(client_id);
    if (it != players_.end()) {
        it->second.update_activity();
    }
            
}
bool GameServer::handle_player_ready_packet(std::string client_id, const uint8_t* packet_data, size_t packet_length) {
    bool is_ready = false;
    if (!PacketValidator::extract_ready_status(packet_data, packet_length, is_ready)) {
        LOG_WARN("Failed to extract ready status from packet");
        return false;
    }
    
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

void GameServer::on_client_disconnect(std::string client_id) {
    // Remove player from map
    auto it = players_.find(client_id);
    if (it != players_.end()) {
        players_.erase(it);
        LOG_INFO("Player removed. Remaining players: %zu", players_.size());
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

    network_service_.broadcast_packet(packet);
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
