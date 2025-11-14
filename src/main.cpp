#define ENET_IMPLEMENTATION
#define _WINSOCK_DEPRECATED_NO_WARNINGS
/* Standard Libraries */
#include <stdio.h>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

/* Third-Party Libraries */
#include "vendor/enet.h"


enum class ERROR_CODE {
    ERROR_NONE,
    ERROR_ENET_INIT_FAILED,
    ERROR_ENET_CREATION_FAILED,
    ERROR_ENET_JOIN_FAILED,
};

enum class GAME_STATE {
    PREGAME,
    ONGOING,
    PAUSED,
    ENDING,
};

struct Player {
    std::string client_id;
    bool is_ready;
};
std::vector<Player> players;

bool lobby_check_ready() {
    // Check if all players are ready
    for (const auto& player : players) {
        if (!player.is_ready) {
            return false;
        }
    }
    return true;
}
int main() {
    printf("Starting Server\n");
    GAME_STATE state = GAME_STATE::PREGAME;
    // Initialize ENet
    if (enet_initialize() != 0) {
        fprintf(stderr, "An error occurred while initializing ENet.\n");
        return (int)ERROR_CODE::ERROR_ENET_INIT_FAILED;
    }
    ENetAddress network_connection = {0};
    // Read .env file if exists or import from system environment variables
    const int env_port = std::stoi(std::getenv("SERVER_PORT") == nullptr ? "0" : std::getenv("SERVER_PORT"));
    const char* players_string = std::getenv("PLAYERS");
// TODO: Implement JSON parsing - cmkrist 13/11/2025
    // Port Config
    if (env_port != 0) {
        printf("Using port from environment: %d\n", env_port);
        network_connection.port = env_port;
    } else {
        printf("No port specified in environment, using default 7000\n");
        network_connection.port = 7000;
    }
    // Players Config
// TODO: Find a way to pass players to the ENet host creation - cmkrist 13/11/2025
    #define MAX_CLIENTS 1
    // Host all (Managed by ContainerService)
    network_connection.host = ENET_HOST_ANY;

    // Initialize ENet server host
    ENetHost* enet_server = enet_host_create(&network_connection, MAX_CLIENTS, 2, 0, 0);
    if (enet_server == nullptr) {
        fprintf(stderr, "An error occurred while trying to create an ENet server host.\n");
        enet_deinitialize();
        return (int)ERROR_CODE::ERROR_ENET_CREATION_FAILED;
    }
    printf("Server started on port: %d\n", network_connection.port);

    // Main server loop
    ENetEvent event;
    while (true) {
        while (enet_host_service(enet_server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    printf("A new client connected from %s:%u.\n",
                           event.peer->address.host,
                           event.peer->address.port);
                    if (state == GAME_STATE::PREGAME && lobby_check_ready()) {
                        // Start the game
                        state = GAME_STATE::ONGOING;
                        printf("All players ready. Starting the game!\n");
                    } else {
                        printf("Waiting for all players to be ready...\n");
                    }
                    break;
                case ENET_EVENT_TYPE_RECEIVE:
                    printf("A packet of length %u was received from %s on channel %u.\n",
                           (unsigned int)event.packet->dataLength,
                           (char*)event.peer->data,
                           (unsigned int)event.channelID);
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("%s disconnected.\n", (char*)event.peer->data);
                    event.peer->data = NULL;
                    break;
                default:
                    break;
            }
        }
    }
    enet_host_destroy(enet_server);
    enet_deinitialize();

    return (int)ERROR_CODE::ERROR_NONE;
}