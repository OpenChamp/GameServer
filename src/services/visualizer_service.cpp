#include <services/visualizer_service.hpp>
#include <systems/entity_manager.hpp>
#include <components/map.hpp>
#include <components/movement.hpp>
#include <components/stats.hpp>
#include <components/entity_state.hpp>
#include <components/intent.hpp>
#include <libs/log.hpp>
#include <sstream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>

// Windows socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SHUT_RDWR SD_BOTH
#endif

// Helper functions to convert enums to strings
static std::string entity_state_to_string(EntityState state) {
    switch (state) {
        case EntityState::SPAWNED: return "SPAWNED";
        case EntityState::IDLE: return "IDLE";
        case EntityState::PATHFINDING_WAITING: return "PATHFINDING_WAITING";
        case EntityState::HOLDING_FOR_TARGET: return "HOLDING_FOR_TARGET";
        case EntityState::MOVING: return "MOVING";
        case EntityState::STOPPING: return "STOPPING";
        case EntityState::STUCK: return "STUCK";
        case EntityState::ATTACKING: return "ATTACKING";
        case EntityState::DEAD: return "DEAD";
        default: return "UNKNOWN";
    }
}

static std::string intent_type_to_string(IntentType type) {
    switch (type) {
        case IntentType::NONE: return "NONE";
        case IntentType::MOVE_TO_POSITION: return "MOVE_TO_POSITION";
        case IntentType::ATTACK_TARGET: return "ATTACK_TARGET";
        default: return "UNKNOWN";
    }
}

VisualizerService::VisualizerService(uint16_t port)
    : port_(port) {
}

VisualizerService::~VisualizerService() {
    stop();
}

void VisualizerService::initialize(EntityManager* entity_manager, Map* map) {
    entity_manager_ = entity_manager;
    map_ = map;
    LOG_INFO("VisualizerService initialized on port %u", port_);
}

bool VisualizerService::start() {
    if (running_) {
        LOG_WARN("VisualizerService already running");
        return false;
    }
    
    running_ = true;
    server_thread_ = std::make_unique<std::thread>(&VisualizerService::run_server, this);
    return true;
}

void VisualizerService::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    if (server_thread_ && server_thread_->joinable()) {
        server_thread_->join();
    }
    
    LOG_INFO("VisualizerService stopped");
}

void VisualizerService::run_server() {
#ifdef _WIN32
    // Initialize Winsock
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        LOG_ERROR("WSAStartup failed");
        running_ = false;
        return;
    }
    
    // Create listening socket
    SOCKET listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_socket == INVALID_SOCKET) {
        LOG_ERROR("socket() failed: %d", WSAGetLastError());
        WSACleanup();
        running_ = false;
        return;
    }
    
    // Set socket to reuse address
    int reuse = 1;
    if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse)) < 0) {
        LOG_WARN("setsockopt(SO_REUSEADDR) failed");
    }
    
    // Bind to port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port_);
    
    if (bind(listen_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        LOG_ERROR("bind() failed: %d", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        running_ = false;
        return;
    }
    
    // Listen for connections
    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        LOG_ERROR("listen() failed: %d", WSAGetLastError());
        closesocket(listen_socket);
        WSACleanup();
        running_ = false;
        return;
    }
    
    LOG_INFO("HTTP server listening on port %u", port_);
    
    // Accept connections loop
    while (running_) {
        // Set a timeout for accept so we can check running_ flag periodically
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(listen_socket, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;  // 1 second timeout
        tv.tv_usec = 0;
        
        int select_result = select((int)listen_socket + 1, &read_fds, nullptr, nullptr, &tv);
        if (select_result < 0) {
            LOG_ERROR("select() failed: %d", WSAGetLastError());
            break;
        }
        
        if (select_result == 0) {
            // Timeout - check if we should continue
            continue;
        }
        
        // Accept connection
        struct sockaddr_in client_addr;
        int client_addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(listen_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        
        if (client_socket == INVALID_SOCKET) {
            LOG_ERROR("accept() failed: %d", WSAGetLastError());
            continue;
        }
        
        // Read HTTP request
        char buffer[4096];
        int recv_len = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (recv_len > 0) {
            buffer[recv_len] = '\0';
            
            // Parse request
            std::string request(buffer);
            std::string response;
            
            // Extract the request path
            std::string path = "/";
            size_t path_start = request.find("GET ");
            if (path_start != std::string::npos) {
                path_start += 4;
                size_t path_end = request.find(" ", path_start);
                if (path_end != std::string::npos) {
                    path = request.substr(path_start, path_end - path_start);
                }
            }
            
            if (path.find("/api/gamestate") != std::string::npos) {
                // Return JSON game state
                std::string json = get_game_state_json();
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Connection: close\r\n";
                response += "\r\n";
                response += json;
            } else {
                // Serve files from web_ui/ directory
                std::string file_path = "web_ui";
                if (path == "/" || path.empty()) {
                    file_path += "/index.html";
                } else {
                    file_path += path;
                }
                
                std::string content = get_file_content(file_path);
                if (!content.empty()) {
                    // Determine content type based on file extension
                    std::string content_type = "text/plain";
                    if (file_path.find(".html") != std::string::npos) {
                        content_type = "text/html";
                    } else if (file_path.find(".css") != std::string::npos) {
                        content_type = "text/css";
                    } else if (file_path.find(".js") != std::string::npos) {
                        content_type = "application/javascript";
                    } else if (file_path.find(".json") != std::string::npos) {
                        content_type = "application/json";
                    }
                    
                    response = "HTTP/1.1 200 OK\r\n";
                    response += "Content-Type: " + content_type + "\r\n";
                    response += "Content-Length: " + std::to_string(content.length()) + "\r\n";
                    response += "Connection: close\r\n";
                    response += "\r\n";
                    response += content;
                } else {
                    // 404 Not Found
                    response = "HTTP/1.1 404 Not Found\r\n";
                    response += "Content-Type: text/plain\r\n";
                    response += "Content-Length: 9\r\n";
                    response += "Connection: close\r\n";
                    response += "\r\n";
                    response += "Not Found";
                }
            }
            
            // Send response
            send(client_socket, response.c_str(), (int)response.length(), 0);
        }
        
        // Close client socket
        shutdown(client_socket, SHUT_RDWR);
        closesocket(client_socket);
    }
    
    // Cleanup
    closesocket(listen_socket);
    WSACleanup();
    LOG_INFO("HTTP server stopped");
#else
    LOG_WARN("VisualizerService HTTP server not implemented for non-Windows platforms");
#endif
    running_ = false;
}

std::string VisualizerService::get_html_page() const {
    // Try to load from file first
    std::ifstream file("web/index.html");
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }
    
    // Fallback: return minimal HTML
    return R"(
<!DOCTYPE html>
<html>
<head><title>GameServer Debugger</title></head>
<body>
<h1>GameServer Debugger</h1>
<p>web/index.html not found. Please ensure visualizer files are in the correct directory.</p>
</body>
</html>
    )";
}

std::string VisualizerService::get_file_content(const std::string& path) const {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string VisualizerService::get_game_state_json() const {
    std::ostringstream json;
    json << std::fixed << std::setprecision(2);
    
    json << "{";
    
    // Map data
    json << "\"map\":{";
    if (map_) {
        json << "\"name\":\"" << map_->name << "\",";
        json << "\"size\":{\"x\":" << map_->size.x << ",\"y\":" << map_->size.y << "},";
        json << "\"offset\":{\"x\":" << map_->offset.x << ",\"y\":" << map_->offset.y << "},";
        
        // Vertices
        json << "\"vertices\":[";
        for (size_t i = 0; i < map_->vertices.size(); i++) {
            if (i > 0) json << ",";
            json << "{\"x\":" << map_->vertices[i].x << ",\"z\":" << map_->vertices[i].y << "}";
        }
        json << "],";
        
        // Spawnpoints
        json << "\"spawnpoints\":[";
        for (size_t i = 0; i < map_->spawnpoints.size(); i++) {
            if (i > 0) json << ",";
            json << "{\"x\":" << map_->spawnpoints[i].x << ",\"z\":" << map_->spawnpoints[i].y << "}";
        }
        json << "],";

        // Structures
        json << "\"structures\":[";
        for (size_t i = 0; i < map_->structures.size(); i++)  {
            if (i > 0) json << ",";
            json << "{";
            json << "\"id\":\"" << map_->structures[i].id << "\",";
            json << "\"type\":\"" << map_->structures[i].type << "\",";
            json << "\"team\":" << map_->structures[i].team << ",";
            json << "\"position\":{\"x\":" << map_->structures[i].position.x << ",\"y\":" << map_->structures[i].position.y << ",\"z\":" << map_->structures[i].position.z << "},";
            json << "\"rotation\":{\"x\":" << map_->structures[i].rotation.x << ",\"y\":" << map_->structures[i].rotation.y << ",\"z\":" << map_->structures[i].rotation.z << "},";
            json << "\"scale\":{\"x\":" << map_->structures[i].scale.x << ",\"y\":" << map_->structures[i].scale.y << ",\"z\":" << map_->structures[i].scale.z << "}";
            json << "}";
        }
        json << "],";

        
        // Polygon Grid (for visualization)
        json << "\"grid\":{";
        json << "\"origin\":{\"x\":" << map_->grid_origin.x << ",\"y\":" << map_->grid_origin.y << "},";
        json << "\"cell_size\":" << map_->grid_cell_size << ",";
        json << "\"width\":" << map_->grid_width << ",";
        json << "\"height\":" << map_->grid_height << ",";
        json << "\"cells\":[";
        if (map_->grid_width > 0 && map_->grid_height > 0) {
            bool first_cell = true;
            for (int y = 0; y < map_->grid_height; y++) {
                for (int x = 0; x < map_->grid_width; x++) {
                    int index = y * map_->grid_width + x;
                    int poly_count = 0;
                    if (index >= 0 && index < (int)map_->grid_cells.size()) {
                        poly_count = (int)map_->grid_cells[index].size();
                    }
                    
                    // Only include cells with polygons
                    if (poly_count > 0) {
                        if (!first_cell) json << ",";
                        first_cell = false;
                        json << "{\"x\":" << x << ",\"y\":" << y << ",\"polygon_count\":" << poly_count << "}";
                    }
                }
            }
        }
        json << "]";
        json << "}";
    }
    json << "},";
    
    // Entities
    json << "\"entities\":[";
    if (entity_manager_) {
        auto all_entities = entity_manager_->get_entities_with_component<Movement>();
        bool first = true;
        for (auto* entity : all_entities) {
            if (!entity) continue;

            auto* move = entity->get_component<Movement>();
            auto* stats = entity->get_component<Stats>();
            auto* state = entity->get_component<EntityStateComponent>();
            auto* intent = entity->get_component<IntentComponent>();

            if (!move || !stats || !state || !intent) continue;
            
            if (!first) json << ",";
            first = false;
            
            json << "{";
            json << "\"id\":" << entity->get_id() << ",";
            json << "\"position\":{\"x\":" << move->position.x << ",\"z\":" << move->position.y << "},";
            json << "\"team_id\":" << (int)(stats ? stats->team_id : 0) << ",";
            json << "\"state\":\"" << (state ? entity_state_to_string(state->current_state) : "UNKNOWN") << "\",";
            json << "\"intent\":\"" << (intent ? intent_type_to_string(intent->type) : "UNKNOWN") << "\"";
            if (intent && intent->target_entity_id != INVALID_ENTITY_ID) {
                json << ",\"target_id\":" << intent->target_entity_id;
            }
            json << "}";
        }
    }
    json << "],";
    
    // State
    json << "\"state\":{\"message\":\"Visualizer active\"}";
    
    json << "}";
    return json.str();
}
