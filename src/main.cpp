#define ENET_IMPLEMENTATION
/* Standard Libraries */
#include <stdio.h>
#include <csignal>
#include <string>
#include <stdexcept>

/* Project Headers */
#include "gameserver.hpp"

// === Configuration Constants ===
constexpr int DEFAULT_PORT = 7000;
constexpr int DEFAULT_MAX_CLIENTS = 1;
constexpr int MAX_PORT = 65535;
constexpr int MIN_PORT = 1;
constexpr int MAX_CLIENTS_LIMIT = 1000;

// Global pointer to server instance for signal handler
static GameServer* g_server_instance = nullptr;

// Signal handler for graceful shutdown
void signal_handler(int signal) {
    if (signal == SIGINT && g_server_instance) {
        printf("\nShutdown signal received. Cleaning up...\n");
        g_server_instance->request_shutdown();
    }
}

int main() {
    printf("OpenChamp GameServer Starting\n");
    printf("==============================\n");
    
    // Parse port from environment
    int env_port = DEFAULT_PORT;
    const char* port_env = std::getenv("SERVER_PORT");
    if (port_env != nullptr) {
        try {
            env_port = std::stoi(port_env);
            if (env_port <= MIN_PORT || env_port > MAX_PORT) {
                fprintf(stderr, "Invalid port number: %s. Using default %d\n", port_env, DEFAULT_PORT);
                env_port = DEFAULT_PORT;
            } else {
                printf("Using port from environment: %d\n", env_port);
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "Failed to parse SERVER_PORT: %s. Using default %d\n", e.what(), DEFAULT_PORT);
            env_port = DEFAULT_PORT;
        }
    } else {
        printf("No port specified in environment, using default %d\n", DEFAULT_PORT);
    }
    
    // Parse max clients from environment
    int max_clients = DEFAULT_MAX_CLIENTS;
    const char* clients_env = std::getenv("MAX_CLIENTS");
    if (clients_env != nullptr) {
        try {
            max_clients = std::stoi(clients_env);
            if (max_clients <= 0 || max_clients > MAX_CLIENTS_LIMIT) {
                fprintf(stderr, "Invalid MAX_CLIENTS: %s. Using default %d\n", clients_env, DEFAULT_MAX_CLIENTS);
                max_clients = DEFAULT_MAX_CLIENTS;
            } else {
                printf("Using MAX_CLIENTS from environment: %d\n", max_clients);
            }
        } catch (const std::exception& e) {
            fprintf(stderr, "Failed to parse MAX_CLIENTS: %s. Using default %d\n", e.what(), DEFAULT_MAX_CLIENTS);
            max_clients = DEFAULT_MAX_CLIENTS;
        }
    } else {
        printf("No MAX_CLIENTS specified in environment, using default %d\n", DEFAULT_MAX_CLIENTS);
    }
    
    // Create and initialize server
    GameServer server(env_port, max_clients);
    
    ERROR_CODE init_result = server.initialize();
    if (init_result != ERROR_CODE::ERROR_NONE) {
        fprintf(stderr, "Failed to initialize server: %d\n", (int)(init_result));
        return (int)(init_result);
    }
    
    // Register signal handler for graceful shutdown
    g_server_instance = &server;
    std::signal(SIGINT, signal_handler);
    
    // Run the server main loop
    server.run();
    // Cleanup happens automatically in GameServer destructor
    printf("Server shutdown complete.\n");
    return (int)(ERROR_CODE::ERROR_NONE);
}