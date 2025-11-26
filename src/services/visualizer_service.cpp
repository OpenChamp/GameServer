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
        case IntentType::MOVE_TO_OBJECTIVE: return "MOVE_TO_OBJECTIVE";
        case IntentType::MOVE_TO_POSITION: return "MOVE_TO_POSITION";
        case IntentType::MOVE_TO_SPAWNPOINT: return "MOVE_TO_SPAWNPOINT";
        case IntentType::ATTACK_TARGET: return "ATTACK_TARGET";
        case IntentType::DEFEND_POSITION: return "DEFEND_POSITION";
        case IntentType::CHASE_ENEMY: return "CHASE_ENEMY";
        case IntentType::RETREAT: return "RETREAT";
        case IntentType::PATROL: return "PATROL";
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
    
    <h3>Visualization Legend</h3>
    <div style="font-family: monospace; font-size: 12px; line-height: 1.8;">
        <div><span style="color: #1a3a1a;">█ Grid</span> - Reference coordinate grid (10-unit spacing)</div>
        <div><span style="color: #664040;">█ Map Bounds</span> - Dashed line showing map boundary</div>
        <div><span style="color: #408040;">█ Navmesh</span> - Walkable area polygons</div>
        <div><span style="color: #80ff80;">● Vertices</span> - Navmesh vertex positions with indices</div>
        <div><span style="color: #ff6b6b;">● Centroids</span> - Polygon center points for pathfinding</div>
        <div><span style="color: #ffff00;">★ Spawnpoints</span> - Unit spawn locations</div>
        <div><span style="color: #4040ff;">● Blue Entity</span> - Team 1 entity</div>
        <div><span style="color: #ff4040;">● Red Entity</span> - Team 2 entity</div>
        <div><span style="color: #ffff00;">⚫ Attacking Ring</span> - Entity in ATTACKING state</div>
        <div><span style="color: #4da6ff;">⚫ Moving Ring</span> - Entity in MOVING state</div>
        <div><span style="color: #ffa500;">⚫ Pathfinding Ring</span> - Entity in PATHFINDING_WAITING state</div>
    </div>
    
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
                
                // Draw grid first (background)
                drawGrid(data.map);
                
                // Draw map bounds
                drawMapBounds(data.map);
                
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
        
        function screenToWorld(screenX, screenY) {
            return {
                x: (screenX - 400) / 10,
                y: (screenY - 300) / 10
            };
        }
        
        function worldToScreen(worldX, worldY) {
            return {
                x: worldX * 10 + 400,
                y: worldY * 10 + 300
            };
        }
        
        function drawGrid(mapData) {
            const gridSize = 10; // Units per grid square
            const gridScale = gridSize * 10; // Pixels per grid square (at 10px/unit)
            
            ctx.strokeStyle = '#1a3a1a';
            ctx.lineWidth = 0.5;
            
            // Vertical lines
            for (let x = 0; x < canvas.width; x += gridScale) {
                ctx.beginPath();
                ctx.moveTo(x, 0);
                ctx.lineTo(x, canvas.height);
                ctx.stroke();
            }
            
            // Horizontal lines
            for (let y = 0; y < canvas.height; y += gridScale) {
                ctx.beginPath();
                ctx.moveTo(0, y);
                ctx.lineTo(canvas.width, y);
                ctx.stroke();
            }
            
            // Draw grid labels every 50 units
            ctx.fillStyle = '#2a4a2a';
            ctx.font = '10px monospace';
            ctx.textAlign = 'right';
            ctx.textBaseline = 'top';
            
            for (let worldX = -100; worldX <= 100; worldX += 50) {
                const screenX = worldToScreen(worldX, 0).x;
                if (screenX >= 0 && screenX <= canvas.width) {
                    ctx.fillText(worldX.toString(), screenX - 3, 3);
                }
            }
            
            ctx.textAlign = 'left';
            for (let worldY = -100; worldY <= 100; worldY += 50) {
                const screenY = worldToScreen(0, worldY).y;
                if (screenY >= 0 && screenY <= canvas.height) {
                    ctx.fillText(worldY.toString(), 3, screenY);
                }
            }
        }
        
        function drawMapBounds(mapData) {
            if (!mapData) return;
            
            // Draw map boundary as a dashed line
            ctx.strokeStyle = '#664040';
            ctx.lineWidth = 2;
            ctx.setLineDash([10, 5]);
            
            const halfWidth = mapData.size.x / 2;
            const halfHeight = mapData.size.y / 2;
            
            const minX = mapData.offset.x - halfWidth;
            const maxX = mapData.offset.x + halfWidth;
            const minY = mapData.offset.y - halfHeight;
            const maxY = mapData.offset.y + halfHeight;
            
            const topLeft = worldToScreen(minX, minY);
            const topRight = worldToScreen(maxX, minY);
            const bottomLeft = worldToScreen(minX, maxY);
            const bottomRight = worldToScreen(maxX, maxY);
            
            ctx.beginPath();
            ctx.moveTo(topLeft.x, topLeft.y);
            ctx.lineTo(topRight.x, topRight.y);
            ctx.lineTo(bottomRight.x, bottomRight.y);
            ctx.lineTo(bottomLeft.x, bottomLeft.y);
            ctx.closePath();
            ctx.stroke();
            
            ctx.setLineDash([]);
        }
        
        function drawNavmesh(mapData) {
            if (!mapData || !mapData.vertices) return;
            
            const vertices = mapData.vertices;
            const polygons = mapData.polygons || [];
            
            // Draw polygon faces
            ctx.strokeStyle = '#408040';
            ctx.fillStyle = '#204020';
            ctx.lineWidth = 2;
            
            for (let poly of polygons) {
                if (poly.length < 3) continue;
                
                ctx.beginPath();
                const v = vertices[poly[0]];
                const screen = worldToScreen(v.x, v.z);
                ctx.moveTo(screen.x, screen.y);
                
                for (let i = 1; i < poly.length; i++) {
                    const v = vertices[poly[i]];
                    const screen = worldToScreen(v.x, v.z);
                    ctx.lineTo(screen.x, screen.y);
                }
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
            }
            
            // Draw vertices with labels
            ctx.fillStyle = '#80ff80';
            ctx.strokeStyle = '#ffffff';
            ctx.lineWidth = 1;
            ctx.font = 'bold 9px monospace';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            
            for (let i = 0; i < vertices.length; i++) {
                const v = vertices[i];
                const screen = worldToScreen(v.x, v.z);
                
                // Draw vertex circle
                ctx.beginPath();
                ctx.arc(screen.x, screen.y, 3, 0, Math.PI * 2);
                ctx.fill();
                ctx.stroke();
                
                // Draw vertex index
                ctx.fillStyle = '#000000';
                ctx.fillText(i.toString(), screen.x, screen.y);
                ctx.fillStyle = '#80ff80';
            }
            
            // Draw polygon centroids
            ctx.fillStyle = '#ff6b6b';
            for (let i = 0; i < polygons.length; i++) {
                const poly = polygons[i];
                if (poly.length < 3) continue;
                
                // Calculate centroid
                let cx = 0, cy = 0;
                for (let idx of poly) {
                    cx += vertices[idx].x;
                    cy += vertices[idx].z;
                }
                cx /= poly.length;
                cy /= poly.length;
                
                const screen = worldToScreen(cx, cy);
                ctx.beginPath();
                ctx.arc(screen.x, screen.y, 2, 0, Math.PI * 2);
                ctx.fill();
            }
        }
        
        function drawSpawnpoints(mapData) {
            if (!mapData || !mapData.spawnpoints) return;
            
            ctx.fillStyle = '#ffff00';
            ctx.strokeStyle = '#ffff00';
            ctx.lineWidth = 2;
            ctx.font = '12px Arial';
            ctx.textAlign = 'center';
            ctx.textBaseline = 'bottom';
            
            for (let i = 0; i < mapData.spawnpoints.length; i++) {
                const sp = mapData.spawnpoints[i];
                const screen = worldToScreen(sp.x, sp.z);
                
                // Draw spawnpoint star
                ctx.beginPath();
                for (let j = 0; j < 5; j++) {
                    const angle = (j * 4 * Math.PI) / 5 - Math.PI / 2;
                    const radius = j % 2 === 0 ? 8 : 4;
                    const x = screen.x + Math.cos(angle) * radius;
                    const y = screen.y + Math.sin(angle) * radius;
                    if (j === 0) ctx.moveTo(x, y);
                    else ctx.lineTo(x, y);
                }
                ctx.closePath();
                ctx.fill();
                ctx.stroke();
                
                // Draw label
                ctx.fillStyle = '#ffff00';
                ctx.fillText('S' + i, screen.x, screen.y - 12);
            }
        }
        
        function drawEntities(entities) {
            if (!entities) return;
            
            // First pass: draw targeting lines
            for (let entity of entities) {
                if (entity.target_id !== undefined) {
                    const attacker_screen = worldToScreen(entity.position.x, entity.position.z);
                    
                    // Find target entity
                    const target = entities.find(e => e.id === entity.target_id);
                    if (target) {
                        const target_screen = worldToScreen(target.position.x, target.position.z);
                        
                        // Draw targeting line
                        ctx.strokeStyle = entity.team_id === 1 ? '#4080ff' : '#ff4080';
                        ctx.lineWidth = 2;
                        ctx.setLineDash([5, 5]);
                        ctx.beginPath();
                        ctx.moveTo(attacker_screen.x, attacker_screen.y);
                        ctx.lineTo(target_screen.x, target_screen.y);
                        ctx.stroke();
                        ctx.setLineDash([]);
                        
                        // Draw arrowhead at target
                        const angle = Math.atan2(target_screen.y - attacker_screen.y, target_screen.x - attacker_screen.x);
                        const arrowSize = 8;
                        
                        ctx.fillStyle = entity.team_id === 1 ? '#4080ff' : '#ff4080';
                        ctx.beginPath();
                        ctx.moveTo(target_screen.x, target_screen.y);
                        ctx.lineTo(target_screen.x - arrowSize * Math.cos(angle - Math.PI / 6), target_screen.y - arrowSize * Math.sin(angle - Math.PI / 6));
                        ctx.lineTo(target_screen.x - arrowSize * Math.cos(angle + Math.PI / 6), target_screen.y - arrowSize * Math.sin(angle + Math.PI / 6));
                        ctx.closePath();
                        ctx.fill();
                    }
                }
            }
            
            // Second pass: draw entities
            for (let entity of entities) {
                const screen = worldToScreen(entity.position.x, entity.position.z);
                
                // Color by team
                ctx.fillStyle = entity.team_id === 1 ? '#4040ff' : '#ff4040';
                ctx.beginPath();
                ctx.arc(screen.x, screen.y, 4, 0, Math.PI * 2);
                ctx.fill();
                
                // Draw ID
                ctx.fillStyle = '#ffffff';
                ctx.font = 'bold 10px Arial';
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText(entity.id.toString(), screen.x, screen.y);
                
                // Highlight if attacking
                if (entity.state === 'ATTACKING') {
                    ctx.strokeStyle = '#ffff00';
                    ctx.lineWidth = 2;
                    ctx.beginPath();
                    ctx.arc(screen.x, screen.y, 6, 0, Math.PI * 2);
                    ctx.stroke();
                }
                
                // Draw state indicator
                let stateColor;
                switch (entity.state) {
                    case 'ATTACKING': stateColor = '#ffff00'; break;
                    case 'MOVING': stateColor = '#4da6ff'; break;
                    case 'PATHFINDING_WAITING': stateColor = '#ffa500'; break;
                    case 'DEAD': stateColor = '#ff0000'; break;
                    default: stateColor = '#ffb347'; break;
                }
                ctx.strokeStyle = stateColor;
                ctx.lineWidth = 1;
                ctx.beginPath();
                ctx.arc(screen.x, screen.y, 8, 0, Math.PI * 2);
                ctx.stroke();
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
                
                let targetStr = '';
                if (entity.target_id !== undefined) {
                    const target = entities.find(e => e.id === entity.target_id);
                    const targetName = target ? `E${entity.target_id}` : `E${entity.target_id} (DEAD)`;
                    targetStr = ` <span style="color: #ff8080">→ Targeting ${targetName}</span>`;
                }
                
                const stateColor = entity.state === 'ATTACKING' ? '#ffff00' : 
                                  entity.state === 'MOVING' ? '#4da6ff' :
                                  entity.state === 'DEAD' ? '#ff0000' : '#ffb347';
                
                el.innerHTML = `<strong>E${entity.id}</strong> [Team ${entity.team_id}] | Intent: <span style="color: #4da6ff">${entity.intent}</span> | State: <span style="color: ${stateColor}"><b>${entity.state}</b></span>${targetStr} | (${entity.position.x.toFixed(1)}, ${entity.position.z.toFixed(1)})`;
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
            auto* intent = entity->get_component<IntentComponent>();
            
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
