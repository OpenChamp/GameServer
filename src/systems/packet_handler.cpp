#include "packet_handler.hpp"
#include "player_manager.hpp"
#include "entity_manager.hpp"
#include "services/network_service.hpp"
#include "systems/packet_validator.hpp"
#include "components/readiness.hpp"
#include "components/network_metadata.hpp"
#include <log.hpp>

PacketHandler::PacketHandler(PlayerManager* player_manager, 
                           EntityManager* entity_manager,
                           NetworkService* network_service)
    : player_manager_(player_manager)
    , entity_manager_(entity_manager)
    , network_service_(network_service) {
}

void PacketHandler::handle_packet(const std::string& client_id, const uint8_t* data, size_t length) {
    // Validate packet structure
    if (!PacketValidator::validate_packet(data, length)) {
        LOG_WARN("Invalid packet received from %s", client_id.c_str());
        return;
    }
    
    // Extract packet type
    PACKET_TYPE packet_type = (PACKET_TYPE)(data[0]);
    
    // Update player activity
    EntityID player_entity_id = player_manager_->get_player_entity_id(client_id);
    if (player_entity_id != INVALID_ENTITY_ID) {
        Entity* player_entity = entity_manager_->get_entity(player_entity_id);
        if (player_entity) {
            auto* metadata = player_entity->get_component<NetworkMetadataComponent>();
            if (metadata) {
                metadata->update_activity();
            }
        }
    }
    
    // Dispatch to appropriate handler
    switch (packet_type) {
        case PACKET_TYPE::PLAYER_READY:
            handle_player_ready_packet(client_id, data, length);
            break;
            
        default:
            handle_other_packets(client_id, (uint8_t)packet_type, data, length);
            break;
    }
}

bool PacketHandler::handle_player_ready_packet(const std::string& client_id, const uint8_t* data, size_t length) {
    bool is_ready = false;
    if (!PacketValidator::extract_ready_status(data, length, is_ready)) {
        LOG_WARN("Failed to extract ready status from packet");
        return false;
    }
    
    // Update player readiness via PlayerManager
    if (!player_manager_->set_player_ready(client_id, is_ready, *entity_manager_)) {
        LOG_WARN("Failed to set player ready status for client: %s", client_id.c_str());
        return false;
    }
    
    LOG_INFO("Player %s is now %s", client_id.c_str(), is_ready ? "ready" : "not ready");
    
    // Note: Game start transition logic should be handled by GameServer/GameplayCoordinator
    // This handler just processes the packet and updates state
    
    return true;
}

bool PacketHandler::handle_other_packets(const std::string& client_id, uint8_t packet_type, 
                                        const uint8_t* data, size_t length) {
    LOG_WARN("Unhandled packet type: %d from client: %s", (int)packet_type, client_id.c_str());
    return false;
}
