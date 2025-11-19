#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <chrono>
#include "components/errors.hpp"
#include "components/game_state.hpp"
#include "components/map.hpp"
#include "libs/frame_timer.h"

// Managers
#include "player_manager.hpp"
#include "packet_handler.hpp"
#include "gameplay_coordinator.hpp"
// Systems
#include "entity_manager.hpp"
// Services
#include "services/navigation_service.hpp"
#include "services/network_service.hpp"
#include "services/visualizer_service.hpp"
/**
 * Central GameServer class encapsulating all server state and logic.
 * Replaces global state management with proper OOP encapsulation.
 */
class GameServer {
public:
    GameServer(int port = 7000, int max_clients = 32, const std::string& map_path = "");
    ~GameServer();
    
    // Prevent copying
    GameServer(const GameServer&) = delete;
    GameServer& operator=(const GameServer&) = delete;
    
    // Allow moving
    GameServer(GameServer&&) = default;
    GameServer& operator=(GameServer&&) = default;
    
    /**
     * Initialize the ENet server and bind to specified port.
     * @return ERROR_CODE indicating success or failure
     */
    ERROR_CODE initialize();
    
    /**
     * Service network events with specified timeout.
     * Processes one event per call.
     * @param timeout_ms Milliseconds to wait for an event
     * @return true if event was processed, false if timeout
     */
        
    /**
     * Main server loop. Processes events until shutdown is requested.
     * Should be called from main() after initialize().
     */
    void run();
    
    /**
     * Request graceful shutdown. Sets the shutdown flag.
     */
    void request_shutdown();
    
    /**
     * Check if shutdown has been requested.
     * @return true if shutdown was requested
     */
    bool is_shutdown_requested() const;
    
    /**
     * Check if all players in lobby are ready.
     * @return true if all connected players have ready=true
     */
    bool is_lobby_ready();
    
    /**
     * Get current game state.
     * @return Current GAME_STATE
     */
    GAME_STATE get_current_state() const;
    
    /**
     * Attempt state transition with validation.
     * @param new_state Target game state
     * @return true if transition was allowed, false otherwise
     */
    bool try_transition_state(GAME_STATE new_state);
    
    /**
     * Get number of connected players.
     * @return Count of players in players map
     */
    size_t get_player_count() const;
    
    /**
     * Get maximum allowed players.
     * @return max_clients value
     */
    int get_max_clients() const;
    
    /**
     * Check if lobby is at capacity.
     * @return true if player_count == max_clients
     */
    bool is_lobby_full() const;
    
    /**
     * Start the debug visualizer web server.
     * @param port Port to listen on (default 8080)
     */
    void start_visualizer(uint16_t port = 8080);
    
private:
    // Network configuration
    int max_clients_;
    std::string map_path_;
    std::unique_ptr<Map> map_pointer_;
    NetworkService network_service_;
    
    // Server configuration
    // Server tick rate in milliseconds
    // TODO this should be configurable, probably - ploinky 14/11/2025
    FrameTimer frame_timer_ = FrameTimer(30);
    
    // Server state
    std::atomic<bool> shutdown_requested_;
    GAME_STATE current_state_;
    
    // Managers
    PlayerManager player_manager_;
    PacketHandler packet_handler_;
    GameplayCoordinator gameplay_;
    
    /**
     * Initialize GameplayCoordinator with required services.
     * Must be called after map and navigation service are initialized.
     */
    void initialize_coordinator();
    
    // Services (Separate thread)
    std::unique_ptr<NavigationService> navigation_service_;
    std::unique_ptr<VisualizerService> debug_visualizer_;

    // ECS systems
    EntityManager entity_manager_;
    Entity* map_entity_ = nullptr;
    
    /**
     * Handle a new client connection.
     * @param client_id The client ID of the connected peer
     */
    void on_client_connect(std::string client_id);
    
    /**
     * Handle an incoming packet.
     * @param client_id The client ID of the peer that sent the packet
     * @param packet The packet the is incoming
     */
    void on_packet_received(std::string client_id, const uint8_t* data, size_t length);
    
    /**
     * Handle client disconnect.
     * @param client_id ID of the client that has disconnected
     */
    void on_client_disconnect(std::string client_id);
    
    /**
     * Perform a single frame tick: process game logic, update states, and broadcast as needed.
     */
    void frame_tick();
    
    /**
     * Validate state transition rules.
     * @param from Source state
     * @param to Destination state
     * @return true if transition is allowed
     */
    bool is_valid_state_transition(GAME_STATE from, GAME_STATE to) const;
};
