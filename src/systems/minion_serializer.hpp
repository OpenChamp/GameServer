#pragma once

#include <vector>
#include <cstdint>
#include "entity_manager.hpp"

/**
 * Serializer for minion state data.
 * Converts minion entities to network packets and vice versa.
 */
class MinionSerializer {
public:
    /**
     * Structure representing a serialized minion for network transmission.
     */
    struct SerializedMinion {
        uint32_t entity_id;
        float x;
        float y;
        float z;
        float health;
        uint8_t state;  // 0=alive, 1=dead, 2=reached_destination
    };
    
    /**
     * Serialize all active minions to an ENet packet.
     * Packet format:
     *   [0]     - PACKET_TYPE::MINION_STATE
     *   [1-4]   - minion count (uint32_t, little-endian)
     *   [5+]    - repeated minion data:
     *     [+0-3]   - entity_id (uint32_t)
     *     [+4-7]   - x position (float)
     *     [+8-11]  - y position (float)
     *     [+12-15] - z position (float)
     *     [+16-19] - health (float)
     *     [+20]    - state (uint8_t)
     * 
     * @param entity_manager Reference to entity manager
     * @return Vector with serialized minion data, or empty vector if no minions
     */
    static std::vector<uint8_t> serialize_minions(EntityManager& entity_manager);
    
    /**
     * Get all active minions from entity manager.
     * @param entity_manager Reference to entity manager
     * @return Vector of serialized minions
     */
    static std::vector<SerializedMinion> collect_minions(EntityManager& entity_manager);
};
