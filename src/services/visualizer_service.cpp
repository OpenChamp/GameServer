#include "visualizer_service.hpp"
#include "entity_manager.hpp"
#include "components/map.hpp"
#include "components/movement.hpp"
#include "components/stats.hpp"
#include "components/entity_state.hpp"
#include <log.hpp>
#include <sstream>
#include <iomanip>
#include <vector>
#include <thread>
#include <chrono>

// Windows socket headers
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define SHUT_RDWR SD_BOTH
#endif

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
            
            if (request.find("GET /api/gamestate") != std::string::npos) {
                // Return JSON game state
                std::string json = get_game_state_json();
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(json.length()) + "\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "Connection: close\r\n";
                response += "\r\n";
                response += json;
            } else if (request.find("GET /") != std::string::npos || request.find("GET") != std::string::npos) {
                // Return HTML page
                std::string html = get_html_page();
                response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: text/html\r\n";
                response += "Content-Length: " + std::to_string(html.length()) + "\r\n";
                response += "Connection: close\r\n";
                response += "\r\n";
                response += html;
            } else {
                // 404 Not Found
                response = "HTTP/1.1 404 Not Found\r\n";
                response += "Content-Type: text/plain\r\n";
                response += "Content-Length: 9\r\n";
                response += "Connection: close\r\n";
                response += "\r\n";
                response += "Not Found";
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
    return R"(
<!DOCTYPE html>
<html>
<head>
    <title>GameServer Debugger</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1e1e1e; color: #d4d4d4; }
        canvas { border: 1px solid #404040; background: #000; margin: 20px 0; display: block; }
        .info { font-family: monospace; white-space: pre; background: #252526; padding: 10px; }
        .entity { margin: 5px 0; }
        h2 { border-bottom: 1px solid #404040; padding-bottom: 10px; }
    </style>
</head>
<body>
    <h1>GameServer Debugger</h1>
    <h2>Map View</h2>
    <canvas id="mapCanvas" width="800" height="600"></canvas>
    
    <h2>Game State</h2>
    <div class="info" id="state"></div>
    
    <h2>Entities</h2>
    <div id="entities"></div>
    
    <script>
        const canvas = document.getElementById('mapCanvas');
        const ctx = canvas.getContext('2d');
        
        async function updateView() {
            try {
                const response = await fetch('/api/gamestate');
                const data = await response.json();
                
                // Clear canvas
                ctx.fillStyle = '#000';
                ctx.fillRect(0, 0, canvas.width, canvas.height);
                
                // Draw navmesh
                drawNavmesh(data.map);
                
                // Draw spawnpoints
                drawSpawnpoints(data.map);
                
                // Draw entities
                drawEntities(data.entities);
                
                // Update state info
                updateStateInfo(data.state);
                
                // Update entities list
                updateEntitiesList(data.entities);
            } catch (e) {
                console.error('Failed to fetch game state:', e);
            }
        }
        
        function drawNavmesh(mapData) {
            if (!mapData || !mapData.vertices) return;
            
            const vertices = mapData.vertices;
            const polygons = mapData.polygons || [];
            
            ctx.strokeStyle = '#408040';
            ctx.fillStyle = '#204020';
            ctx.lineWidth = 1;
            
            for (let poly of polygons) {
                if (poly.length < 3) continue;
                
                ctx.beginPath();
                const v = vertices[poly[0]];
                ctx.moveTo(v.x * 10 + 400, v.z * 10 + 300);
                
                for (let i = 1; i < poly.length; i++) {
                    const v = vertices[poly[i]];
                    ctx.lineTo(v.x * 10 + 400, v.z * 10 + 300);
                }
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
            }
        }
        
        function drawSpawnpoints(mapData) {
            if (!mapData || !mapData.spawnpoints) return;
            
            ctx.fillStyle = '#ffff00';
            ctx.font = '12px Arial';
            
            for (let i = 0; i < mapData.spawnpoints.length; i++) {
                const sp = mapData.spawnpoints[i];
                const x = sp.x * 10 + 400;
                const y = sp.z * 10 + 300;
                
                ctx.beginPath();
                ctx.arc(x, y, 5, 0, Math.PI * 2);
                ctx.fill();
                ctx.fillText('S' + i, x - 5, y - 10);
            }
        }
        
        function drawEntities(entities) {
            if (!entities) return;
            
            for (let entity of entities) {
                const x = entity.position.x * 10 + 400;
                const y = entity.position.z * 10 + 300;
                
                // Color by team
                ctx.fillStyle = entity.team_id === 1 ? '#4040ff' : '#ff4040';
                ctx.beginPath();
                ctx.arc(x, y, 4, 0, Math.PI * 2);
                ctx.fill();
                
                // Draw ID
                ctx.fillStyle = '#ffffff';
                ctx.font = '10px Arial';
                ctx.fillText(entity.id, x - 10, y - 8);
            }
        }
        
        function updateStateInfo(state) {
            document.getElementById('state').textContent = JSON.stringify(state, null, 2);
        }
        
        function updateEntitiesList(entities) {
            const div = document.getElementById('entities');
            div.innerHTML = '';
            
            if (!entities) return;
            
            for (let entity of entities) {
                const el = document.createElement('div');
                el.className = 'entity';
                el.innerHTML = `<strong>Entity ${entity.id}</strong> | Team ${entity.team_id} | State: ${entity.state} | Pos: (${entity.position.x.toFixed(1)}, ${entity.position.z.toFixed(1)})`;
                div.appendChild(el);
            }
        }
        
        // Update every 500ms
        setInterval(updateView, 500);
        updateView();
    </script>
</body>
</html>
    )";
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
        json << "]";
    }
    json << "},";
    
    // Entities
    json << "\"entities\":[";
    if (entity_manager_) {
        auto all_entities = entity_manager_->get_entities_with_component<Movement>();
        bool first = true;
        for (auto* entity : all_entities) {
            if (!first) json << ",";
            first = false;
            
            auto* move = entity->get_component<Movement>();
            auto* stats = entity->get_component<Stats>();
            auto* state = entity->get_component<EntityStateComponent>();
            
            json << "{";
            json << "\"id\":" << entity->get_id() << ",";
            json << "\"position\":{\"x\":" << move->position.x << ",\"z\":" << move->position.y << "},";
            json << "\"team_id\":" << (int)(stats ? stats->team_id : 0) << ",";
            json << "\"state\":\"" << (state ? std::to_string((int)state->current_state) : "UNKNOWN") << "\"";
            json << "}";
        }
    }
    json << "],";
    
    // State
    json << "\"state\":{\"message\":\"Visualizer active\"}";
    
    json << "}";
    return json.str();
}
