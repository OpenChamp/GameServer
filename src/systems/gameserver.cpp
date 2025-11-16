#include "gameserver.hpp"
#include "packet_validator.hpp"
#include <optional>
#include <log.hpp>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <vector>

GameServer::GameServer(int port, int max_clients, const std::string& map_path)
    : max_clients_(max_clients)
    , shutdown_requested_(false)
    , map_path_(map_path)
    , map_pointer_(nullptr)
    , navigation_service_(nullptr)
    , current_state_(GAME_STATE::PREGAME)
    , last_minion_broadcast_(std::chrono::high_resolution_clock::now())
    , network_service_(NetworkService(port, max_clients)) {
    network_service_.on_client_connected = [this](std::string client_id) { on_client_connect(client_id); };
    network_service_.on_client_disconnected = [this](std::string client_id) { on_client_disconnect(client_id); };
    network_service_.on_packet_received = [this](std::string client_id, const uint8_t* data, size_t length) { on_packet_received(client_id, data, length); };
    network_service_.start_server();
        
}

GameServer::~GameServer() {
    if(network_service_.is_connected()) {
        network_service_.disconnect();
    }
    LOG_INFO("GameServer destroyed");
}

ERROR_CODE GameServer::initialize() {
    // Initialize Map
    std::optional<Map> map_opt = MapSystem::load_map(map_path_);
    if (!map_opt) {
        LOG_ERROR("Failed to load map");
        return ERROR_CODE::ERROR_ENET_CREATION_FAILED;
    }

    map_pointer_ = new Map(std::move(map_opt.value()));

    // Initialize Navigation
    navigation_service_ = std::make_unique<NavigationService>(std::move(map_opt.value()));

    return ERROR_CODE::ERROR_NONE;
}

void GameServer::run() {
    LOG_INFO("Starting server main loop");
    
    while (!shutdown_requested_) {
        // Frame timing
        if(!frame_timer_.is_frame()) {
            continue;
        }

        if(network_service_.is_connected()) {
            network_service_.run_callbacks();
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
    if (map_pointer_) {
        network_service_.send_packet(PACKET_TYPE::SPAWN_MAP, map_pointer_->name, client_id);
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

void GameServer::on_packet_received(std::string client_id, const uint8_t* data, size_t length) {
    // Validate and process packet
    if (!PacketValidator::validate_packet(data, length)) {
        LOG_WARN("Invalid packet received from %s", client_id.c_str());
        return;
    }
    PACKET_TYPE packet_type = (PACKET_TYPE)(data[0]);
    
    switch (packet_type) {
        case PACKET_TYPE::PLAYER_READY:
            handle_player_ready_packet(client_id, data, length);
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
        if(try_transition_state(GAME_STATE::ONGOING)) {
            // Notify all players that the game is starting
            network_service_.broadcast_packet(PACKET_TYPE::GAME_START);
        }
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
