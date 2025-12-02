#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <cstdint>

// Forward declarations
class EntityManager;
struct SystemContext;

/**
 * VisualizerService - Optional web-based visualization of game state
 * 
 * RESPONSIBILITIES:
 *   - Serve a web page for visualizing map and entities
 *   - Provide JSON API endpoints for game state
 *   - Run HTTP server in background thread
 *   - Display real-time entity positions and states
 * 
 * USAGE (optional, debug only):
 *   VisualizerService visualizer(8080);  // Port 8080
 *   visualizer.initialize(&entity_manager, map_pointer_.get());
 *   visualizer.start();
 *   // Open http://localhost:8080 in browser
 *   
 * NOTE: This is entirely optional and only for development debugging.
 * Disable with VISUALIZER_ENABLED = false or by not initializing.
 */
class VisualizerService {
public:
    VisualizerService(uint16_t port = 8080);
    ~VisualizerService();
    
    // Prevent copying
    VisualizerService(const VisualizerService&) = delete;
    VisualizerService& operator=(const VisualizerService&) = delete;
    
    // Allow moving
    VisualizerService(VisualizerService&&) = default;
    VisualizerService& operator=(VisualizerService&&) = default;
    
    /**
     * Initialize visualizer with entity manager and map.
     * Must be called before start().
     * @param entity_manager Pointer to entity manager
     * @param map Map data for visualization
     */
    void initialize(EntityManager* entity_manager, struct Map* map);
    
    /**
     * Start the HTTP server in background thread.
     * @return true if server started successfully
     */
    bool start();
    
    /**
     * Stop the HTTP server and wait for thread to finish.
     */
    void stop();
    
    /**
     * Check if server is running.
     * @return true if HTTP server is active
     */
    bool is_running() const { return running_; }
    
    /**
     * Get the port the server is listening on.
     * @return Port number
     */
    uint16_t get_port() const { return port_; }

private:
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::unique_ptr<std::thread> server_thread_;
    EntityManager* entity_manager_ = nullptr;
    struct Map* map_ = nullptr;
    
    /**
     * Run the HTTP server (called in background thread).
     */
    void run_server();
    
    /**
     * Get HTML page content.
     * @return HTML string for visualization
     */
    std::string get_html_page() const;
    
    /**
     * Get file content from disk.
     * @param path Path to file relative to working directory
     * @return File content or empty string if not found
     */
    std::string get_file_content(const std::string& path) const;
    
    /**
     * Get current game state as JSON.
     * @return JSON string with map, entities, and state
     */
    std::string get_game_state_json() const;
};
