#pragma once

#include "component.hpp"
#include <string>

/**
 * ClientInfoComponent - Player network identification
 * 
 * USAGE: Add to player entities only
 * SYSTEMS: PlayerManager, NetworkService
 * REQUIRED COMPANIONS: ReadinessComponent, NetworkMetadataComponent
 * 
 * Maps network client_id to in-game player_id for identification
 * across server systems.
 */
struct ClientInfoComponent : public Component {
    std::string client_id;      // Unique network identifier
    uint32_t player_id;         // Server-assigned player ID
    
    COMPONENT_TYPE_ID(ClientInfoComponent, 3001)
};
