#include <systems/gameserver.hpp>
#include <systems/system_context.hpp>
#include <optional>
#include <libs/log.hpp>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <chrono>
#include <vector>

#include <components/movement.hpp>

#include <systems/core/wave_system.hpp>
#include <systems/util/data_loader.hpp>
#include <systems/util/packet_validator.hpp>

GameServer::GameServer(int port, int max_clients, const std::string& map_path)
    : max_clients_(max_clients)
    , shutdown_requested_(false)
    , map_path_(map_path)
    , map_pointer_(nullptr)
    , navigation_service_(nullptr)
    , current_state_(GAME_STATE::PREGAME)
    , network_service_(NetworkService(port, max_clients))
    , player_manager_()
    , packet_handler_(&player_manager_, &entity_manager_, &network_service_, &gameplay_.get_input_system())
    , gameplay_() {

}

GameServer::~GameServer() {
    if(network_service_.is_connected()) {
        network_service_.disconnect();
    }
    LOG_INFO("GameServer destroyed");
}

ERROR_CODE GameServer::initialize() {
    // Load map using DataLoader
    std::optional<Map> map_opt = DataLoader::load_map(map_path_);
    if (!map_opt) {
        LOG_ERROR("Failed to load map");
        return ERROR_CODE::ERROR_ENET_CREATION_FAILED;
    }
    
    // Initialize Navigation
    navigation_service_ = std::make_unique<NavigationService>(map_opt.value());
    
    // Create pointer copy
    map_pointer_ = std::make_unique<Map>(std::move(map_opt.value()));
    
    // Initialize Network
    network_service_.on_client_connected = [this](std::string client_id) { on_client_connect(client_id); };
    network_service_.on_client_disconnected = [this](std::string client_id) { on_client_disconnect(client_id); };
    network_service_.on_packet_received = [this](std::string client_id, const uint8_t* data, size_t length) { on_packet_received(client_id, data, length); };
    ERROR_CODE net_result = network_service_.start_server();
    if (net_result != ERROR_CODE::ERROR_NONE) {
        LOG_ERROR("Failed to start network service");
        return net_result;
    }
    
    // Initialize GameplayCoordinator Systems
    initialize_coordinator();

    // Autostart logic (Unlimited players) -- cmkrist 4/12/
    if (max_clients_ == 0) {
        LOG_INFO("max_players set to 0, transitioning straight to ONGOING state");
        try_transition_state(GAME_STATE::ONGOING);
    }
    // Default lobby ready check
    else if (is_lobby_full() && is_lobby_ready()) {
        LOG_INFO("Lobby full and ready on startup, transitioning to ONGOING state");
        try_transition_state(GAME_STATE::ONGOING);
    }
    
    return ERROR_CODE::ERROR_NONE;
}

void GameServer::run() {
    LOG_INFO("Starting server main loop");
    
    while (!shutdown_requested_) {
        // Service network events
        if (network_service_.is_connected()) {
            network_service_.run_callbacks();
        }
        // Frame timing
        if(!frame_timer_.is_frame()) {
            continue;
        }
        GameServer::frame_tick();
    }
    LOG_INFO("Server main loop ended");
}

void GameServer::frame_tick() {
    float delta_time_ms = frame_timer_.frame_duration_in_ms();
    
    // Create system context with all services
    SystemContext ctx(entity_manager_);
    ctx.input_system = &gameplay_.get_input_system();
    ctx.navigation_service = navigation_service_.get();
    ctx.network_service = &network_service_;
    ctx.map = map_pointer_.get();
    ctx.delta_time_ms = delta_time_ms;
    
    // Update all game systems through coordinator
    if (current_state_ == GAME_STATE::ONGOING) {
        gameplay_.update(ctx);
    }
}

void GameServer::request_shutdown() {
    LOG_INFO("Shutdown requested");
    shutdown_requested_ = true;
}

bool GameServer::is_shutdown_requested() const {
    return shutdown_requested_;
}

bool GameServer::is_lobby_ready() {
    return player_manager_.are_all_players_ready(entity_manager_);
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
    return player_manager_.get_player_count();
}

int GameServer::get_max_clients() const {
    return max_clients_;
}

bool GameServer::is_lobby_full() const {
    return player_manager_.is_full(max_clients_);
}

void GameServer::on_client_connect(std::string client_id) {
    // Lobby check & Map Send
    if (max_clients_ > 0 && player_manager_.is_full(max_clients_)) {
        LOG_WARN("Lobby full! Rejecting new connection from %s", client_id.c_str());
        network_service_.disconnect_client(client_id);
        network_service_.send_packet(PACKET_TYPE::LOBBY_FULL, client_id);
        return;
    }
    EntityID player_entity_id = player_manager_.on_client_connect(client_id, entity_manager_);

    if (map_pointer_) {
        network_service_.send_packet(PACKET_TYPE::MAP_LOAD, map_pointer_->name, client_id);
    }

    LOG_INFO("Player %s added. Entity ID: %u",
            client_id.c_str(), player_entity_id);
    
    if (max_clients_ == 0) {
        LOG_INFO("max_players set to 0 (unlimited), player connected");
    } else if (player_manager_.is_full(max_clients_)) {
        LOG_INFO("Lobby full! Waiting for all players to be ready");
    } else {
        LOG_INFO("Waiting for more players...");
    }
}

void GameServer::on_packet_received(std::string client_id, const uint8_t* data, size_t length) {
    // Delegate to PacketHandler for processing
    packet_handler_.handle_packet(client_id, data, length);
    
    // Default lobby ready check with autostart -- cmkrist 4/12/2025
    bool should_start_game = false;
    if (current_state_ == GAME_STATE::PREGAME) {
        if (max_clients_ == 0) {
            should_start_game = true;
        } else if (player_manager_.is_full(max_clients_) && 
                   player_manager_.are_all_players_ready(entity_manager_)) {
            should_start_game = true;
        }
    }
    
    if (should_start_game) {
        LOG_INFO("All players ready! Transitioning to ONGOING state");
        if (try_transition_state(GAME_STATE::ONGOING)) {
            // Notify all players that the game is starting
            network_service_.broadcast_packet(PACKET_TYPE::GAME_START);
        }
    }
}

void GameServer::on_client_disconnect(std::string client_id) {
    // Remove player entity via PlayerManager
    bool was_removed = player_manager_.on_client_disconnect(client_id, entity_manager_);
    
    if (was_removed) {
        LOG_INFO("Player %s disconnected", client_id.c_str());
    }
    
    // Handle state changes if game was ongoing
    if (current_state_ == GAME_STATE::ONGOING && player_manager_.get_player_count() == 0) {
        LOG_WARN("All players disconnected. Returning to PREGAME");
        try_transition_state(GAME_STATE::PREGAME);
    }
}

void GameServer::initialize_coordinator() {
    // Set up WaveSystem with all required services
    gameplay_.initialize_wave_system(&entity_manager_, &network_service_, navigation_service_.get(), map_pointer_.get());
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

void GameServer::start_visualizer(uint16_t port) {
    if (!map_pointer_) {
        LOG_ERROR("Cannot start visualizer: map not initialized");
        return;
    }
    
    debug_visualizer_ = std::make_unique<VisualizerService>(port);
    debug_visualizer_->initialize(&entity_manager_, map_pointer_.get());
    if (debug_visualizer_->start()) {
        LOG_INFO("Debug visualizer started on port %u", port);
    } else {
        LOG_ERROR("Failed to start debug visualizer");
    }
}
