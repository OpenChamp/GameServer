#define ENET_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS
/* Standard Libraries */
#include <stdio.h>
#include <csignal>
#include <string>
#include <stdexcept>

/* Project Headers */
#include <systems/gameserver.hpp>

// === Configuration Constants ===
constexpr int DEFAULT_PORT = 7000;
constexpr int DEFAULT_MAX_CLIENTS = 1;
constexpr int MAX_PORT = 65535;
constexpr int MIN_PORT = 7000;
constexpr int MAX_CLIENTS_LIMIT = 1000;

// Global pointer to server instance for signal handler
static GameServer* g_server_instance = nullptr;

// Signal handler for graceful shutdown

// === Helper Functions === //
static std::string get_map_path_from_env() {
    // Static Path
    const char* map_path_env = std::getenv("MAP_FILE_PATH");
    if (map_path_env != nullptr) {
        std::string map_path(map_path_env);
        LOG_INFO("Using map file path from environment: %s", map_path.c_str());
        return map_path;
    }
    // Map Name
    const char* map_name_env = std::getenv("MAP_NAME");
    if (map_name_env != nullptr) {
        LOG_INFO("Using map name from environment: %s", map_name_env);
        return std::string(map_name_env);
    }
    // Default
    LOG_INFO("No map file path or name specified in environment, using default map");
    return "";
}


void signal_handler(int signal) {
    if (signal == SIGINT && g_server_instance) {
        LOG_INFO("Shutdown signal received. Cleaning up...");
        g_server_instance->request_shutdown();
    }
}

// === Main Entry Point === //
int main(int argc, char* argv[]) {
    LOG_INFO("OpenChamp GameServer Starting");
    LOG_INFO("==============================");
    
    // Parse command-line arguments
    bool enable_visualizer = false;
    uint16_t visualizer_port = 8080;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (arg == "--visualize") {
            enable_visualizer = true;
            LOG_INFO("Visualizer enabled (will run on port %u)", visualizer_port);
        } else if (arg == "--visualize-port" && i + 1 < argc) {
            try {
                visualizer_port = std::stoi(argv[++i]);
                LOG_INFO("Visualizer port set to %u", visualizer_port);
            } catch (...) {
                LOG_ERROR("Invalid visualizer port, using default %u", visualizer_port);
            }
        }
    }
    
    // Parse port from environment
    int env_port = DEFAULT_PORT;
    const char* port_env = std::getenv("SERVER_PORT");
    if (port_env != nullptr) {
        try {
            env_port = std::stoi(port_env);
            if (env_port <= MIN_PORT || env_port > MAX_PORT) {
                LOG_ERROR("Invalid port number: %s. Using default %d", port_env, DEFAULT_PORT);
                env_port = DEFAULT_PORT;
            } else {
                LOG_INFO("Using port from environment: %d\n", env_port);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse SERVER_PORT: %s. Using default %d", e.what(), DEFAULT_PORT);
            env_port = DEFAULT_PORT;
        }
    } else {
        LOG_INFO("No port specified in environment, using default %d", DEFAULT_PORT);
    }
    
    // Parse max clients from environment
    int max_clients = DEFAULT_MAX_CLIENTS;
    const char* clients_env = std::getenv("MAX_CLIENTS");
    if (clients_env != nullptr) {
        try {
            max_clients = std::stoi(clients_env);
            if (max_clients <= 0 || max_clients > MAX_CLIENTS_LIMIT) {
                LOG_ERROR("Invalid MAX_CLIENTS: %s. Using default %d", clients_env, DEFAULT_MAX_CLIENTS);
                max_clients = DEFAULT_MAX_CLIENTS;
            } else {
                LOG_INFO("Using MAX_CLIENTS from environment: %d", max_clients);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to parse MAX_CLIENTS: %s. Using default %d", e.what(), DEFAULT_MAX_CLIENTS);
            max_clients = DEFAULT_MAX_CLIENTS;
        }
    } else {
        LOG_INFO("No MAX_CLIENTS specified in environment, using default %d", DEFAULT_MAX_CLIENTS);
    }
    
    // Create and initialize server
    GameServer server(env_port, max_clients, get_map_path_from_env());
    
    ERROR_CODE init_result = server.initialize();
    if (init_result != ERROR_CODE::ERROR_NONE) {
        LOG_ERROR("Failed to initialize server: %d", (int)(init_result));
        return (int)(init_result);
    }
    
    // Start visualizer if requested
    if (enable_visualizer) {
        server.start_visualizer(visualizer_port);
    }
    
    // Register signal handler for graceful shutdown
    g_server_instance = &server;
    std::signal(SIGINT, signal_handler);
    
    // Run the server main loop
    server.run();
    // Cleanup happens automatically in GameServer destructor
    LOG_INFO("Server shutdown complete.");
    return (int)(ERROR_CODE::ERROR_NONE);
}